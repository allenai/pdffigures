#include <cmath>
#include <algorithm>

#include "BuildCaptions.h"

namespace {

// Debugging method
void printLine(std::vector<TextWord *> line, const char *header) {
  printf("%s:", header);
  for (size_t i = 0; i < line.size(); ++i) {
    printf("%s ", line.at(i)->getText()->getCString());
  }
  printf("\n");
}

// Gather all the TextWord objects in a page
std::vector<TextWord *> collectWords(TextPage *page) {
  std::vector<TextWord *> words = std::vector<TextWord *>();
  TextFlow *flow = page->getFlows();
  while (flow != NULL) {
    TextBlock *block = flow->getBlocks();
    while (block != NULL) {
      TextLine *line = block->getLines();
      while (line != NULL) {
        TextWord *word = line->getWords();
        while (word != NULL) {
          words.push_back(word);
          word = word->getNext();
        }
        line = line->getNext();
      }
      block = block->getNext();
    }
    flow = flow->getNext();
  }
  return words;
}

// How text in a caption can be formatted
enum Alignment { CENTERED, L_ALIGNED, UNKNOWN };

// Represent a (possibly partially built) caption
class CaptionRegion {
public:
  CaptionRegion(double x, double y, double x2, double y2, double xLimit)
      : x(x), y(y), x2(x2), y2(y2), xLimit(xLimit), numLines(0),
        alignment(UNKNOWN) {
    words = std::vector<TextWord *>();
  }

  void addWord(TextWord *word) {
    words.push_back(word);
    double wx, wy, wx2, wy2;
    word->getBBox(&wx, &wy, &wx2, &wy2);
    x2 = std::max(wx2, x2);
    y2 = std::max(wy2, y2);
    x = std::min(wx, x);
    y = std::min(wy, y);
  }

  void addLine(std::vector<TextWord *> words) {
    for (size_t i = 0; i < words.size(); ++i) {
      addWord(words.at(i));
    }
    numLines += 1;
  }

  // Bounding box
  double x;
  double y;
  double x2;
  double y2;

  double xLimit;
  int numLines;
  Alignment alignment;
  std::vector<TextWord *> words;
};

// Point where a word starts
typedef std::pair<double, double> EdgeLocation;
EdgeLocation getEdge(TextWord *word) {
  double x, y, x2, y2;
  word->getBBox(&x, &y, &x2, &y2);
  return EdgeLocation(x, (y + y2) / 2.0);
}

/**
   Returns a vector of word edges that we are sure should not be included as
   part of a caption. We use this to help know when stop 'expanding' captions
   to the right.
 */
std::vector<EdgeLocation> getParagraphEdges(std::vector<TextWord *> &words,
                                            std::vector<CaptionStart> starts) {
  std::vector<EdgeLocation> paragraphEdges = std::vector<EdgeLocation>();
  for (size_t i = 0; i < starts.size(); ++i) {
    paragraphEdges.push_back(getEdge(starts.at(i).word));
  }
  return paragraphEdges;
  /**
     We could be clever and try to find paragraph edges by seeking blocks of
     x-aligned words in practice it seems like this is not needed

    for (int i = 0; i < words.size(); ++i) {
      if (std::find(paragraphEdges.begin(), paragraphEdges.end(), words.at(i))
    != paragraphEdges.end())
        continue;
      double px, py, px2, py2;
      words.at(i)->getBBox(&px, &py, &px2, &py2);
      std::vector<TextWord*> xAlignedWords = std::vector<TextWord*>();
      for (int j = 0; j < words.size(); ++j) {
        double x, y, x2, y2;
        words.at(j)->getBBox(&x, &y, &x2, &y2);
        if (abs(x - px) < 2 and py2 < y) {
          if (std::find(paragraphEdges.begin(), paragraphEdges.end(),
    words.at(i)) == paragraphEdges.end()) {
            xAlignedWords.push_back(words.at(j));
          }
        }
      }

      std::vector<TextWord*> alignedWords = std::vector<TextWord*>();
      bool foundWord = false;
      while (foundWord) {
        foundWord = false;
        for (int j = 0; j < xAlignedWords.size(); ++j) {
          double x, y, x2, y2;
          xAlignedWords.at(j)->getBBox(&x, &y, &x2, &y2);
          if (y - py2 < 5) {
            alignedWords.push_back(xAlignedWords.at(j));
            py2 = y2;
            foundWord = true;
          }
        }
      }
      if (alignedWords.size() > 3) {
        paragraphEdges.insert(paragraphEdges.end(), alignedWords.begin(),
    alignedWords.end());
      }
    }
    return paragraphEdges; */
}

/**
   Given a starting x value and limiting x value, adds words from yAlignedWords
   to lineWords as long as doing so would not create an overly large 'jump'
   between words.
 */
double extendLineRightFromCandidates(double x2, double xLimit,
                                     std::vector<TextWord *> &yAlignedWords,
                                     std::vector<TextWord *> &lineWords) {
  bool foundCandidate = false;
  do {
    foundCandidate = false;
    for (size_t j = 0; j < yAlignedWords.size(); ++j) {
      double wx, wy, wx2, wy2;
      yAlignedWords.at(j)->getBBox(&wx, &wy, &wx2, &wy2);
      if ((wx - x2) < 20 and wx > (x2 - 2) and wx < xLimit and wx2 > x2) {
        lineWords.push_back(yAlignedWords.at(j));
        x2 = wx2;
        foundCandidate = true;
        break;
      }
    }
  } while (foundCandidate);
  return x2;
}

/**
   Returns the largest x coordinate of the word that is not a paragraph edge and
   does not come after a paragraphic edge found inside paragraphEdges.
 */
double getHorizontalLimit(std::vector<TextWord *> yAlignedWords,
                          std::vector<EdgeLocation> &paragraphEdges) {
  double maxX = 99999999;
  for (size_t j = 0; j < yAlignedWords.size(); ++j) {
    if (std::find(paragraphEdges.begin(), paragraphEdges.end(),
                  getEdge(yAlignedWords.at(j))) != paragraphEdges.end()) {
      double wx, wy, wx2, wy2;
      yAlignedWords.at(j)->getBBox(&wx, &wy, &wx2, &wy2);
      maxX = std::min(maxX, wx);
    }
  }
  return maxX;
}

// Extract words that are found between after x and between y and y2
std::vector<TextWord *> getYAlignedWords(double x, double y, double y2,
                                         std::vector<TextWord *> &words) {
  std::vector<TextWord *> yAlignedWords = std::vector<TextWord *>();
  for (size_t j = 0; j < words.size(); ++j) {
    double wx, wy, wx2, wy2;
    words.at(j)->getBBox(&wx, &wy, &wx2, &wy2);
    int tol = 4;
    double cy = (wy + wy2) / 2.0;
    if ((cy + tol) > y and (cy - tol) < y2 and wx > x) {
      yAlignedWords.push_back(words.at(j));
    }
  }
  return yAlignedWords;
}

// Adds a line to region, return false iff no line could be found.
bool addLine(std::vector<TextWord *> &words, BOXA *graphicBoxes,
             std::vector<EdgeLocation> &paragraphEdges, CaptionRegion &region,
             int verbose) {

  if (region.alignment != CENTERED) {
    double wx, wy, wx2, wy2;
    region.words.back()->getBBox(&wx, &wy, &wx2, &wy2);
    GooString *lastStr = region.words.back()->getText();
    if ((region.x2 - wx2) > 60 and
        lastStr->getChar(lastStr->getLength() - 1) == '.') {
      if (verbose >= 2)
        printf("Ragged edge\n");
      return false;
    }
  }

  double x = region.x;
  double x2 = region.x2;
  double y2 = region.y2;
  TextWord *word = NULL;
  double startX2 = -1;
  double startX = -1;
  double startY2 = -1;
  double startY = -1;
  std::vector<TextWord *> yAlignedWords = std::vector<TextWord *>();
  for (size_t j = 0; j < words.size(); ++j) {
    double wx, wy, wx2, wy2;
    words.at(j)->getBBox(&wx, &wy, &wx2, &wy2);
    if ((wy - y2) < 8 and (wy - y2) > -3 and (wx2 - x) > -5 and wy2 - 1 > y2) {
      yAlignedWords.push_back(words.at(j));
      if (word == NULL or wx < startX) {
        word = words.at(j);
        startX2 = wx2;
        startX = wx;
        startY2 = wy2;
        startY = wy;
      }
    }
  }

  if (word == NULL)
    return false;

  if (verbose >= 2)
    printLine(yAlignedWords, "Candidates");
  double xLimit = getHorizontalLimit(yAlignedWords, paragraphEdges);
  std::vector<TextWord *> line = std::vector<TextWord *>();
  line.push_back(word);
  startX2 = extendLineRightFromCandidates(
      startX2, std::min(region.xLimit, xLimit), yAlignedWords, line);
  if (verbose >= 2)
    printLine(line, "Proposed Line");

  bool leftAligned = std::abs(startX - x) < 2;
  bool centered = (std::abs((x2 + x) / 2.0 - (startX2 + startX) / 2.0)) < 2;
  if (not leftAligned and not centered) {
    if (verbose >= 2)
      printf("Not aligned or centered\n");
    return false;
  }

  if (region.alignment == UNKNOWN and not(leftAligned and centered)) {
    region.alignment = centered ? CENTERED : L_ALIGNED;
  }

  // Do not allow lines that would cross graphical lines / boxes
  for (int i = 0; i < graphicBoxes->n; ++i) {
    BOX *gb = graphicBoxes->box[i];
    int w = std::min(gb->x + gb->w, (int)(startX2 + 0.5)) -
            std::max(gb->x, (int)(startX + 0.5));
    if (w < 0)
      continue;
    int h = std::min(gb->y + gb->h, (int)(startY2 + 0.5)) -
            std::max(gb->y, (int)(startY));
    if ((h >= 0 and h < 15 and ((w / float(startX2 - startX)) > 0.90)) or
        (w * h > 8000)) {
      if (verbose >= 2)
        printf("Graphic confict (%d, %d, %0.2f)\n", h, h * w,
               (w / float(startX2 - startX)));
      return false;
    }
  }

  // New lines should be left or center aligned with the old line
  if ((centered and region.alignment != L_ALIGNED) or
      (leftAligned and region.alignment != CENTERED)) {
    region.addLine(line);
    region.xLimit = std::min(region.xLimit, xLimit);
    if (verbose >= 2)
      printf("Accepted\n");
    return true;
  }
  if (verbose >= 2)
    printf("Does not match the caption region's alignment\n");
  return false;
}

// Builds a caption from CaptionStart
Caption buildCaption(CaptionStart start, DocumentStatistics &docStats,
                     std::vector<TextWord *> &allWords,
                     std::vector<EdgeLocation> &paragraphEdges,
                     BOXA *graphicBoxes, int verbose) {
  double x, y, x2, y2;
  start.word->getBBox(&x, &y, &x2, &y2);
  std::vector<TextWord *> words = std::vector<TextWord *>();
  words.push_back(start.word);
  std::vector<TextWord *> yAlignedWords = getYAlignedWords(x, y, y2, allWords);
  double xLimit = getHorizontalLimit(yAlignedWords, paragraphEdges);
  x2 = extendLineRightFromCandidates(x2, xLimit, yAlignedWords, words);
  CaptionRegion captionRegion = CaptionRegion(x, y, x2, y2, xLimit);
  captionRegion.addLine(words);

  if (verbose >= 2)
    printf("On region: %s%d\n", getFigureTypeString(start.type), start.number);
  if (verbose >= 2)
    printLine(words, "First Line");

  bool foundLine = true;
  while (foundLine) {
    foundLine =
        addLine(allWords, graphicBoxes, paragraphEdges, captionRegion, verbose);
  }
  return Caption(start.page, start.number, start.type,
                 boxCreate(captionRegion.x - 1.5, captionRegion.y - 1.5,
                           captionRegion.x2 - captionRegion.x + 2.5,
                           captionRegion.y2 - captionRegion.y + 2.5));
}

} // end namespace

std::vector<Caption> buildCaptions(std::vector<CaptionStart> &starts,
                                   DocumentStatistics &docStats, TextPage *text,
                                   PIX *graphics, int verbose) {
  std::vector<Caption> captions = std::vector<Caption>();
  std::vector<TextWord *> words = collectWords(text);
  std::vector<EdgeLocation> paragraphEdges = getParagraphEdges(words, starts);
  BOXA *graphicBoxes = pixConnCompBB(graphics, 8);
  for (size_t i = 0; i < starts.size(); ++i) {
    captions.push_back(buildCaption(starts.at(i), docStats, words,
                                    paragraphEdges, graphicBoxes, verbose));
  }
  boxaDestroy(&graphicBoxes);
  return captions;
}

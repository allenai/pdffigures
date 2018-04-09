#include <regex>

#include <PDFDoc.h>

#include "TextUtils.h"

void printTextProperties(TextPage *page, DocumentStatistics *docStats,
                         bool onlyLineStarts) {
  std::vector<TextLine *> lines = getLines(page);
  for (TextLine *line : lines) {
    if (docStats->lineIsBold(line)) {
      printf("\nBOLD LINE\n");
    }
    TextWord *word = line->getWords();
    while (word != NULL) {
      int end = word->getLength() - 1;
      TextFontInfo *fi = word->getFontInfo(end);
      printf("%s-%s: FS: %0.2f, Font: %s,"
             "FLAGS:%s%s%s%s, Large:%s, Itatlic:%s, Bold: %s\n",
             word->getText()->getCString(),
             word->getNext() == NULL ? "NULL"
                                     : word->getNext()->getText()->getCString(),
             word->getFontSize(), word->getFontName(end)->getCString(),
             fi->isBold() ? "T" : "F", fi->isItalic() ? "T" : "F",
             fi->isSerif() ? "T" : "F", fi->isSymbolic() ? "T" : "F",
             docStats->wordIsLarge(word) ? "T" : "F",
             wordIsItalic(word) ? "T" : "F", wordIsBold(word) ? "T" : "F");
      if (onlyLineStarts) {
        word = NULL;
      } else {
        word = word->getNext();
      }
    }
  }
}

void getTop2Values(std::unordered_map<double, int> map, double *first,
                   double *second) {
  int firstCount = -1;
  int secondCount = -1;
  *first = -1;
  *second = -1;
  for (auto &kv : map) {
    if (kv.second > firstCount) {
      *second = *first;
      secondCount = firstCount;
      firstCount = kv.second;
      *first = kv.first;
    } else if (kv.second > secondCount) {
      *second = kv.first;
      secondCount = kv.second;
    }
  }
  if (*first > *second) {
    double tmp = *first;
    *first = *second;
    *first = tmp;
  }
}

const std::regex integerRegex = std::regex("^[0-9]{1,3}$");
const std::regex decimalRegex = std::regex("^[0-9]+(\\.[0-9]+)?$");

DocumentStatistics::DocumentStatistics(std::vector<TextPage *> &textPages,
                                       PDFDoc *doc, bool verbose) {

  if (verbose)
    printf("\nAnalyzing Document...\n");
  std::unordered_map<std::string, double> fontNameCounts =
      std::unordered_map<std::string, double>();
  std::unordered_map<double, double> fontSizeCounts =
      std::unordered_map<double, double>();

  boldCentersUp = std::unordered_map<int, int>();
  boldCentersDown = std::unordered_map<int, int>();

  pageHeaders = std::unordered_map<std::string, int>();
  rMarginCounts = std::unordered_map<double, int>();
  lMarginCounts = std::unordered_map<double, int>();
  totalWords = 0;
  totalLines = 0;
  int pageNumbers = 0;
  for (size_t i = 0; i < textPages.size(); ++i) {
    TextPage *page = textPages.at(i);
    int minY = 99999;
    int maxY = -1;
    TextLine *botLine = NULL;
    TextLine *topLine = NULL;
    std::vector<TextLine *> lines = getLines(page);

    totalLines += lines.size();
    for (TextLine *line : lines) {
      double x, y, x2, y2;
      getTextLineBB(line, &x, &y, &x2, &y2);
      if (y < minY) {
        minY = y;
        topLine = line;
      }
      if (y2 > maxY) {
        maxY = y2;
        botLine = line;
      }
      int centerUp = ((int)(1 + (x + x2) / 2.0));
      int centerDown = ((int)((x + x2) / 2.0));
      x = (double)((int)(x + 0.5));
      x2 = (double)((int)(x2 + 0.5));
      lMarginCounts[x] += 1;
      rMarginCounts[x2] += 1;
      TextWord *word = line->getWords();
      bool isBold = wordIsBold(word);
      bool isDecimal = regex_match(word->getText()->getCString(), decimalRegex);
      while (word != NULL) {
        totalWords += 1;
        fontSizeCounts[word->getFontSize()] += 1;
        std::string fontName;
        if (word->getFontName(word->getLength() - 1) != NULL) {
          fontName = word->getFontName(word->getLength() - 1)->getCString();
        } else {
          fontName = "NULL";
        }
        fontNameCounts[fontName] += 1;
        isBold = wordIsBold(word) and isBold;
        word = word->getNext();
      }
      if (isBold and not isDecimal) {
        boldCentersUp[centerUp] += 1;
        boldCentersDown[centerDown] += 1;
      }
    }

    if (i > 0 and topLine != NULL) {
      if (botLine->getWords()->getNext() == NULL and
          regex_match(botLine->getWords()->getText()->getCString(),
                      integerRegex)) {
        pageNumbers += 1;
      }
      double center = doc->getPageMediaWidth(i) / 2 * (100 / 72.0);
      double x = 0, y = 0, x2 = 0, y2 = 0;
      getTextLineBB(topLine, &x, &y, &x2, &y2);
      if (std::abs((x2 + x) / 2 - center) < 20) {
        TextWord *firstWord = topLine->getWords();
        std::string firstLineText = "";
        while (firstWord != NULL) {
          firstLineText += firstWord->getText()->getCString();
          firstWord = firstWord->getNext();
        }
        pageHeaders[firstLineText] += 1;
      }
    }
  }

  hasPageNumbers = pageNumbers > 5 and pageNumbers > (textPages.size() * .70);
  if (hasPageNumbers and verbose) {
    printf("%d page numbers (%d)\n", pageNumbers, (int)textPages.size());
  }

  modeFontName = fontNameCounts.begin()->first;
  for (auto &fnc : fontNameCounts) {
    if (fontNameCounts[modeFontName] < fnc.second) {
      modeFontName = fnc.first;
    }
  }

  modeFont = fontSizeCounts.begin()->first;
  for (auto &fsc : fontSizeCounts) {
    if (fontSizeCounts[modeFont] < fsc.second) {
      modeFont = fsc.first;
    }
  }

  for (std::unordered_map<std::string, int>::iterator it = pageHeaders.begin();
       it != pageHeaders.end();) {
    if (it->second <= 2) {
      pageHeaders.erase(it++);
    } else {
      if (verbose)
        printf("Found Page Header: <%s> (%d)\n", it->first.c_str(), it->second);
      ++it;
    }
  }

  getTop2Values(lMarginCounts, &lMarginFirst, &lMarginSecond);
  getTop2Values(rMarginCounts, &rMarginFirst, &rMarginSecond);

  double diff = (lMarginCounts[lMarginFirst] - lMarginCounts[lMarginSecond]) /
                ((double)totalLines);
  twoColumn = diff < 0.20 and diff > -0.20;
  diff = (lMarginCounts[lMarginFirst] - lMarginCounts[lMarginSecond]) /
         ((double)totalLines);
  rightAligned = diff < 0.15 and diff > -0.15;

  if (verbose) {
    printf("Margin: \n\t(%0.2f,%d)-(%0.2f,%d) \n\t(%0.2f,%d)-(%0.2f,%d)\n",
           lMarginFirst, lMarginCounts[lMarginFirst], rMarginFirst,
           rMarginCounts[rMarginFirst], lMarginSecond,
           lMarginCounts[lMarginSecond], rMarginSecond,
           rMarginCounts[rMarginSecond]);
    printf("%s Column (%0.2f)\n", twoColumn ? "Two" : "One", diff);
    printf("%s\n", rightAligned ? "Right Aligned" : "Not Right Aligned");
    printf("Analysis Complete.\n\n");
  }

  // Detect if the PDF has its text also included as images
  // by checking to see if an image fills up each page
  imageFilled = true;
  for (int i = 0; i < doc->getNumPages(); ++i) {
    imageFilled = isFilledByImage(doc, i + 1) and imageFilled;
  }
  if (imageFilled and verbose) {
    printf(
        "Graphical elements appear to include body text, ignoring graphics\n");
  }
}

bool DocumentStatistics::isBodyTextGraphical() { return imageFilled; }

bool DocumentStatistics::documentIsTwoColumn() { return twoColumn; }

bool DocumentStatistics::isBoldCentered(double x, double x2) {
  if (lineIsAligned(x, x2) > 0) {
    return true;
  }
  if (rightAligned) {
    double center = (x + x2) / 2.0;
    double columnCenter;
    if (twoColumn and center > rMarginFirst) {
      columnCenter = (lMarginSecond + rMarginSecond) / 2.0;
    } else {
      columnCenter = (lMarginFirst + rMarginFirst) / 2.0;
    }
    return (std::abs(center - columnCenter) <= 2);
  }
  int centerUp = ((int)1 + (x + x2) / 2.0);
  int centerDown = ((int)(x + x2) / 2.0);
  return boldCentersUp[centerUp] + boldCentersDown[centerDown] >= 3;
}

bool DocumentStatistics::isPageHeader(TextLine *line) {
  std::string lineText = "";
  TextWord *word = line->getWords();
  while (word != NULL) {
    lineText += word->getText()->getCString();
    word = word->getNext();
  }
  return pageHeaders.find(lineText) != pageHeaders.end();
}

bool DocumentStatistics::isPageNumber(TextLine *line) {
  if (hasPageNumbers) {
    if (line->getWords()->getNext() == NULL and
        regex_match(line->getWords()->getText()->getCString(), integerRegex)) {
      return true;
    }
  }
  return false;
  ;
}

bool DocumentStatistics::wordIsLarge(TextWord *word) {
  return word->getFontSize() > modeFont;
}

double DocumentStatistics::getModeFont() { return modeFont; }

bool DocumentStatistics::wordIsStandardFont(TextWord *word) {
  return (word->getFontName(word->getLength() - 1) == NULL &&
          modeFontName == "NULL") ||
         (modeFontName.compare(
              word->getFontName(word->getLength() - 1)->getCString()) == 0);
}

int DocumentStatistics::lineIsAligned(double x, double x2) {
  return lineIsAlignedToTol(x, x2, 1, 1);
}

int DocumentStatistics::lineIsAlignedToTol(double x, double x2, double l_tol,
                                           double r_tol) {
  x = (double)((int)(x + 0.5));
  x2 = (double)((int)(x2 + 0.5));
  int score = 0;
  if ((lMarginFirst - x) <= l_tol and (x - lMarginFirst) <= r_tol) {
    score = 1;
  } else if (twoColumn and (lMarginSecond - x) <= l_tol and
             (x - lMarginSecond) <= r_tol) {
    score = 1;
  }
  return score;
}

bool DocumentStatistics::lineIsBold(TextLine *line) {
  TextWord *word = line->getWords();
  if (not wordIsBold(word) or getModeFont() > word->getFontSize() or
      word->getRotation() != 0) {
    return false;
  }
  TextWord *next = word->getNext();
  while (next != NULL) {
    if (next->getFontSize() != word->getFontSize() and wordIsBold(next) and
        next->getRotation() != 0) {
      return false;
    }
    next = next->getNext();
  }
  return true;
}

std::vector<TextLine *> getLines(TextPage *textPage) {
  std::vector<TextLine *> lines = std::vector<TextLine *>();
  TextFlow *flow = textPage->getFlows();
  while (flow != NULL) {
    TextBlock *block = flow->getBlocks();
    while (block != NULL) {
      TextLine *line = block->getLines();
      while (line != NULL) {
        lines.push_back(line);
        line = line->getNext();
      }
      block = block->getNext();
    }
    flow = flow->getNext();
  }
  return lines;
}

void getTextLineBB(TextLine *line, double *minX, double *minY, double *maxX,
                   double *maxY) {
  TextWord *word = line->getWords();
  word->getBBox(minX, minY, maxX, maxY);
  word = word->getNext();
  while (word != NULL) {
    double x, y, x2, y2;
    word->getBBox(&x, &y, &x2, &y2);
    *minX = std::min(x, *minX);
    *minY = std::min(y, *minY);
    *maxX = std::max(x2, *maxX);
    *maxY = std::max(y2, *maxY);
    word = word->nextWord();
  }
}

const std::regex italicFontRegex = std::regex(".*(Slant|Itatlic).*");

bool wordIsItalic(TextWord *const word) {
  if (word->getFontInfo(word->getLength() - 1) != NULL and
      word->getFontInfo(word->getLength() - 1)->isItalic())
    return true;
  if (word->getFontName(word->getLength() - 1) == NULL) {
    return false;
  }
  std::match_results<const char *> italicFontMatch;
  std::regex_match(word->getFontName(word->getLength() - 1)->getCString(),
                   italicFontMatch, italicFontRegex);
  return not italicFontMatch.empty();
}

const std::regex boldFontRegex = std::regex(".*(Medi|Bold).*");

bool wordIsBold(TextWord *const word) {
  if (word->getFontInfo(word->getLength() - 1) != NULL and
      word->getFontInfo(word->getLength() - 1)->isBold())
    return true;
  if (word->getFontName(word->getLength() - 1) == NULL) {
    return false;
  }
  std::match_results<const char *> boldFontMatch;
  std::regex_match(word->getFontName(word->getLength() - 1)->getCString(),
                   boldFontMatch, boldFontRegex);
  return not boldFontMatch.empty();
}

bool wordEndsWithPeriod(TextWord *const word) {
  return *word->getChar(word->getLength() - 1) == Unicode('.');
}

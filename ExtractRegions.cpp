#include <unordered_map>
#include <regex>

#include "TextUtils.h"
#include "ExtractRegions.h"

// TODO minimal memory mangement in this section

// Draw the region into background
PIX *PageRegions::drawRegions(PIX *background) {
  PIX *scratch2 = pixPaintBoxa(background, bodytext, 0);
  PIX *scratch = pixDrawBoxa(scratch2, other, 4, 0xff000000);
  pixDestroy(&scratch2);
  if (graphics->n > 0) {
    scratch2 = pixDrawBoxa(scratch, graphics, 4, 0x00ff0000);
    pixDestroy(&scratch);
  } else {
    scratch2 = scratch;
  }
  BOXA *captionsBoxes = getCaptionsBoxa();
  scratch = pixPaintBoxa(scratch2, captionsBoxes, 0x0000ff00);
  boxaDestroy(&captionsBoxes);
  pixDestroy(&scratch2);
  return scratch;
}

// Create a BOXA of the caption boxes
BOXA *PageRegions::getCaptionsBoxa() {
  BOXA *boxes = boxaCreate((int)captions.size());
  for (Caption caption : captions) {
    boxaAddBox(boxes, caption.boundingBox, L_COPY);
  }
  return boxes;
}

namespace {

const std::regex titleNumberRegex =
    std::regex("^[0-9]{1,2}(\\.[0-9]{1,3})?\\.?$");

// Pulls lines that appear to be titles from lines
std::vector<TextLine *> getTitleLines(std::vector<TextLine *> &lines, int page,
                                      DocumentStatistics &docStats,
                                      bool verbose) {
  std::vector<TextLine *> lineStarts = std::vector<TextLine *>();
  std::vector<TextLine *> boldLines = std::vector<TextLine *>();
  std::vector<TextLine *> titles = std::vector<TextLine *>();

  if (page == 0) {
    // Cheap heurstic, text above the abstract is title,
    // otherwise author names ect. at top can cause problems
    int abstractY = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
      TextWord *word = lines.at(i)->getWords();
      if (strcmp(word->getText()->getCString(), "Abstract") == 0) {
        double x, y, x2, y2;
        word->getBBox(&x, &y, &x2, &y2);
        abstractY = y;
        break;
      }
    }
    if (abstractY != -1) {
      for (size_t i = 0; i < lines.size(); ++i) {
        double x, y, x2, y2;
        lines.at(i)->getWords()->getBBox(&x, &y, &x2, &y2);
        if (y == abstractY or y < abstractY - 10) {
          titles.push_back(lines.at(i));
          lines.erase(lines.begin() + i);
          --i;
        }
      }
    }
  }

  int minY = 99999;
  int topLine = -1;
  int maxY = -1;
  int botLine = -1;
  for (size_t i = 0; i < lines.size(); ++i) {
    double x = 0, y = 0, x2 = 0, y2 = 0;
    getTextLineBB(lines.at(i), &x, &y, &x2, &y2);
    if (y < minY) {
      minY = y;
      topLine = i;
    }
    if (y2 > maxY) {
      maxY = y2;
      botLine = i;
    }
  }

  // Check for page numbers and page header
  if (docStats.isPageNumber(lines.at(botLine))) {
    titles.push_back(lines.at(botLine));
    lines.erase(lines.begin() + botLine);
    if (botLine < topLine)
      --topLine;
  }
  if (docStats.isPageHeader(lines.at(topLine))) {
    titles.push_back(lines.at(topLine));
    lines.erase(lines.begin() + topLine);
  }

  // Look for bold lines that are in places that might indicate
  // they are titles
  size_t i = 0;
  while (i < lines.size()) {
    TextLine *line = lines.at(i);
    if (not docStats.lineIsBold(line)) {
      ++i;
      continue;
    }

    TextWord *word = line->getWords();
    bool isTitleStart =
        regex_match(word->getText()->getCString(), titleNumberRegex);
    double x = 0, y = 0, x2 = 0, y2 = 0;
    getTextLineBB(line, &x, &y, &x2, &y2);

    if (docStats.isBoldCentered(x, x2) and
        not(word->getNext() == NULL and word->getText()->getLength() <= 4) and
        word->getFontSize() > docStats.getModeFont()) {
      TextLine *nextLine = line->getNext();
      if (nextLine != NULL) {
        double nx, ny, nx2, ny2;
        nextLine->getWords()->getBBox(&nx, &ny, &nx2, &ny2);
        if (std::abs(ny - y2) < 10 or std::abs(nx - x2) > 20) {
          nextLine = NULL;
        }
      }
      if (nextLine == NULL) {
        titles.push_back(line);
        lines.erase(lines.begin() + i);
      } else {
        ++i;
      }
      continue;
    }
    if (isTitleStart and docStats.lineIsAlignedToTol(x, x2, 4, 4) == 0) {
      ++i;
      continue;
    }
    if ((word->getNext() == NULL) and isTitleStart) {
      lineStarts.push_back(line);
      titles.push_back(line);
      lines.erase(lines.begin() + i);
    } else if (isTitleStart) {
      lines.erase(lines.begin() + i);
      titles.push_back(line);
    } else {
      ++i;
      boldLines.push_back(line);
    }
  }
  if (verbose) {
    printf("Found %d titles, %d title starts, %d bold lines\n",
           (int)titles.size(), (int)lineStarts.size(), (int)boldLines.size());
  }

  // Try to match line starts with bold lines
  for (TextLine *line : lineStarts) {
    double x, y, x2, y2;
    getTextLineBB(line, &x, &y, &x2, &y2);
    for (size_t j = 0; j < boldLines.size(); ++j) {
      double lx, ly, lx2, ly2;
      getTextLineBB(boldLines.at(j), &lx, &ly, &lx2, &ly2);
      if (std::abs(ly - y) <= 1 and x2 < lx and x2 - lx2 < 50) {
        titles.push_back(boldLines.at(j));
        lines.erase(std::find(lines.begin(), lines.end(), boldLines.at(j)));
        boldLines.erase(boldLines.begin() + j);
        break;
      }
    }
  }

  if (verbose)
    printf("Found a total of %d title boxes\n", (int)titles.size());
  return titles;
}

} // end namespace

PageRegions getPageRegions(PIX *original, TextPage *text, PIX *graphics,
                           const std::vector<Caption> &captions,
                           DocumentStatistics &docStats, int page, bool verbose,
                           bool showSteps, std::vector<Figure> &errors) {

  PIX *scratch;
  PIXA *steps = showSteps ? steps = pixaCreate(4) : NULL;

  std::vector<TextLine *> lines = getLines(text);

  BOXA *otherText = boxaCreate((int)lines.size());
  BOXA *bodyText = boxaCreate((int)lines.size());
  BOXA *graphicBoxes = boxaCreate(0);

  // Identify caption boxes
  BOXA *captionBoxes = boxaCreate((int)captions.size());
  for (Caption capt : captions) {
    boxaAddBox(captionBoxes, capt.boundingBox, L_CLONE);
  }

  const int l_pad = 0;
  const int r_pad = 0;
  const int h_pad = 3;
  PIX *pix1;
  std::vector<TextLine *> titles =
      getTitleLines(lines, page, docStats, verbose);
  for (TextLine *title : titles) {
    double x, y, x2, y2;
    getTextLineBB(title, &x, &y, &x2, &y2);
    boxaAddBox(bodyText, boxCreate(x - 3, y - 3, x2 - x + 6, y2 - y + 6),
               L_CLONE);
  }

  for (TextLine *line : lines) {
    bool rotated = line->getWords()->getRotation() != 0;
    TextWord *word = line->getWords();
    // Loop over words in the line
    while (word != NULL) {
      double lineX, lineY, lineX2, lineY2;
      word->getBBox(&lineX, &lineY, &lineX2, &lineY2);
      BOX *wordBox = boxCreate(lineX + 1, lineY + 1, lineX2 - lineX - 1,
                               lineY2 - lineY - 1);
      int contains;
      for (int i = 0; i < captionBoxes->n; ++i) {
        boxContains(captionBoxes->box[i], wordBox, &contains);
        if (contains)
          break;
      }
      if (contains) {
        word = word->getNext();
        continue;
      }

      bool small = docStats.getModeFont() > word->getFontSize() + 4;
      // Loop over words we want to group into a single text box
      while (word != NULL) {
        double x, y, x2, y2;
        word->getBBox(&x, &y, &x2, &y2);
        BOX *wordBox = boxCreate(x + 0.5, y + 0.5, x2 - x + 0.5, y2 - y + 0.5);
        int contains;
        for (int i = 0; i < captionBoxes->n; ++i) {
          boxContains(captionBoxes->box[i], wordBox, &contains);
          if (contains)
            break;
        }
        if (contains) {
          word = word->getNext();
          continue;
        }
        small = small and (docStats.getModeFont() > word->getFontSize() + 3);
        lineX = std::min(x, lineX);
        lineY = std::min(y, lineY);
        lineX2 = std::max(x2, lineX2);
        lineY2 = std::max(y2, lineY2);
        word = word->getNext();
      }
      if (rotated or small) {
        boxaAddBox(graphicBoxes,
                   boxCreate(lineX + 0.5, lineY + 0.5, lineX2 - lineX + 0.5,
                             lineY2 - lineY + 0.5),
                   L_CLONE);
      } else {
        boxaAddBox(otherText,
                   boxCreate(lineX - l_pad + 0.5, lineY - h_pad + 0.5,
                             lineX2 - lineX + l_pad + 0.5 + r_pad,
                             lineY2 - lineY + 2 * h_pad + 0.5),
                   L_CLONE);
      }
    }
  }

  pix1 = pixCreate(original->w, original->h, 1);
  PIX *textMask = pixMaskBoxa(NULL, pix1, otherText, L_SET_PIXELS);
  textMask = pixConvertTo1(textMask, 250);
  pixDestroy(&pix1);

  // Helps seperate text seperated by images
  pixSubtract(textMask, textMask, graphics);
  if (steps != NULL) {
    pix1 = pixDrawBoxa(original, otherText, 2, 0);
    pixaAddPix(steps, pixDrawBoxa(pix1, bodyText, 2, 0x0000ff00), L_CLONE);
    pixDestroy(&pix1);
  }

  PIXA *ccs = pixaCreate(0);
  otherText = pixConnComp(textMask, &ccs, 4);

  if (showSteps) { // Add the original with the boxes outlined
    scratch = pixPaintBoxa(pixCreateTemplate(original), otherText, 0);
    pixaAddPix(steps, pixPaintBoxa(scratch, bodyText, 0x0000000), L_CLONE);
    pixDestroy(&scratch);
  }

  // Get the graphic regions
  if (showSteps)
    pixaAddPix(steps, graphics, L_COPY);
  BOXA *tmp = pixConnCompBB(graphics, 8);
  boxaJoin(graphicBoxes, tmp, 0, tmp->n);
  scratch = pixMaskBoxa(NULL, pixCreateTemplate(graphics), graphicBoxes,
                        L_SET_PIXELS);
  PIX *graphicMask = pixConvertTo1(scratch, 250);
  if (showSteps)
    pixaAddPix(steps, graphicMask, L_COPY);
  pixDestroy(&scratch);

  // Handle cases where textBox overlaps a captionStarts box by splitting up
  // the offending text box, normally caused by 'notches' in text.
  bool foundSplit = false;
  for (int i = 0; i < captionBoxes->n; ++i) {
    int onBox = 0;
    int toBox = otherText->n;
    while (onBox < toBox) {
      float overlap;
      boxOverlapFraction(otherText->box[onBox], captionBoxes->box[i], &overlap);
      if (overlap > 0.50) {
        foundSplit = true;
        if (verbose)
          printf("Splitting up a text box due to caption overlap\n");
        BOXA *split_bb = pixSplitComponentIntoBoxa(
            pixaGetPix(ccs, onBox, L_CLONE), otherText->box[onBox], 10, 2, 15,
            50, 7, 0);
        // Assume surronding box is bodyText
        boxaJoin(bodyText, split_bb, 0, split_bb->n);
        boxaDestroy(&split_bb);
        boxaRemoveBox(otherText, onBox);
        toBox--;
      } else {
        onBox += 1;
      }
    }
  }
  if (showSteps and foundSplit) {
    scratch = pixDrawBoxa(original, otherText, 5, 0);
    scratch = pixDrawBoxa(scratch, bodyText, 5, 0);
    pixaAddPix(steps, pixDrawBoxa(scratch, captionBoxes, 5, 0), L_CLONE);
    pixDestroy(&scratch);
  }

  // Handle lines over the top
  if (graphicBoxes->n > 0) {
    graphicBoxes = boxaSort(graphicBoxes, L_SORT_BY_Y, L_SORT_INCREASING, NULL);
    BOX *box = boxaGetBox(graphicBoxes, 0, L_CLONE);
    if (box->y < 50 and box->h < 5 and box->w / ((double)original->w) > 0.70) {
      boxaAddBox(bodyText, box, L_COPY);
      boxaRemoveBox(graphicBoxes, 0);
      if (verbose)
        printf("Found top line\n");
    }
  }

  // Classify boxes
  BOXA *other = boxaCreate(0);
  for (int i = 0; i < otherText->n; ++i) {
    BOX *curBox = otherText->box[i];
    float graphicOverlap;
    pixAverageInRect(graphicMask, curBox, &graphicOverlap);
    bool isBody;
    if (graphicOverlap > 0.40) {
      isBody = false;
    } else if (not docStats.isBodyTextGraphical() and
               docStats.lineIsAligned(curBox->x, curBox->x + curBox->w) != 0) {
      isBody = true;
      if (curBox->w * curBox->h < 600) {
        isBody = false;
      } else if ((curBox->w < 150 and docStats.documentIsTwoColumn()) or
                 ((curBox->w < 200 and not docStats.documentIsTwoColumn()) and
                  (curBox->h > curBox->w * 2 or curBox->h > 50))) {
        isBody = false;
      } else {
        isBody = true;
      }
    } else if (docStats.isBodyTextGraphical()) {
      // Cannot rely on document level margins, use size heuristic instead
      if (curBox->w * curBox->h < 15000) {
        isBody = false;
      } else if ((curBox->w < 150 and docStats.documentIsTwoColumn()) or
                 ((curBox->w < 200 and not docStats.documentIsTwoColumn()) and
                  (curBox->h > curBox->w * 2 or curBox->h > 50))) {
        isBody = false;
      } else {
        isBody = true;
      }
    } else {
      isBody = false;
    }
    if (not isBody) {
      boxaAddBox(other, curBox, L_CLONE);
    } else {
      boxaAddBox(bodyText, curBox, L_CLONE);
    }
  }

  // Filter graphic boxes
  for (int i = 0; i < graphicBoxes->n; ++i) {
    BOX *graphicBox = graphicBoxes->box[i];
    if (graphicBox->w > 50 or graphicBox->h > 50) {
      continue;
    }
    for (int j = 0; j < bodyText->n; ++j) {
      float overlap;
      boxOverlapFraction(bodyText->box[j], graphicBox, &overlap);
      if (overlap > 0.80) {
        boxaRemoveBox(graphicBoxes, i);
        --i;
        break;
      }
    }
  }

  PageRegions regions(captions, bodyText, graphicBoxes, other);

  if (showSteps) { // Add the graphics with the boxes outlined
    pixaInsertPix(steps, 0, pixConvertTo1(original, 250), NULL);
    pixaAddPix(steps, regions.drawRegions(pixCreateTemplate(original)),
               L_CLONE);

    BOX *clip;
    PIXA *show = pixaCreate(steps->n);
    pixClipBoxToForeground(pixConvertTo1(original, 250), NULL, NULL, &clip);
    int pad = 10;
    clip->x -= 10;
    clip->y -= 10;
    clip->w += pad * 2;
    clip->h += pad * 2;
    for (int i = 0; i < steps->n; ++i) {
      pixaAddPix(show, pixClipRectangle(steps->pix[i], clip, NULL), L_CLONE);
    }
    boxDestroy(&clip);
    pixDisplay(pixaDisplayTiled(pixaConvertTo32(show), 3000, 1, 30), 0, 0);
    pixaDestroy(&show);
    pixaDestroy(&steps);
  }
  return regions;
}

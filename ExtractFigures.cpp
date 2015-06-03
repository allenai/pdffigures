#include "TextUtils.h"
#include "ExtractFigures.h"

namespace {

int splitBoxVertical(PIX *original, BOX *region) {
  BOX centerSplit = BOX{region->x, region->y + region->h / 2 - 3, region->w, 5};
  int empty = 0;
  std::unique_ptr<PIX> rectangle;
  std::unique_ptr<BOX> split;
  for (int i = 0; i < (region->h * 1) / 4; i++) {
    for (int d = -1; d < 2; d += 2) {
      split.reset(boxCopy(&centerSplit));
      split->y += i * d;
      rectangle.reset(pixClipRectangle(original, split.get(), NULL));
      pixZero(rectangle.get(), &empty);
      if (empty == 1)
        break;
    }
    if (empty == 1)
      break;
  }

  if (empty == 1) {
    int center = split->y + split->h / 2;
    return center;
  } else {
    return -1;
  }
}

/*
 Detect if a box2 is below, above, left, or right of box1
 */
void boxAlignment(BOX *box1, Box *box2, l_int32 tolerance, l_int32 *horizontal,
                  l_int32 *vertical) {
  if (box1->x + box1->w + tolerance <= box2->x) {
    *horizontal = -1;
  } else if (box1->x >= box2->x + box2->w + tolerance) {
    *horizontal = 1;
  } else {
    *horizontal = 0;
  }
  if (box1->y + box1->h + tolerance <= box2->y) {
    *vertical = -1;
  } else if (box1->y >= box2->y + box2->h + tolerance) {
    *vertical = 1;
  } else {
    *vertical = 0;
  }
}

/*
 Expand box left and right as far as possible without it overlapping
 any box in boxes it is not already overlapping. Assumes at least one
 box in boxes is horizontally aligned with box.
 */
void boxExpandLR(BOX *box, BOXA *boxes) {
  int l = 99999, r = 99999;
  for (int j = 0; j < boxes->n; j++) {
    int horizontal = 0, vertical = 0;
    BOX *box2 = boxes->box[j];
    boxAlignment(box, box2, 0, &horizontal, &vertical);
    if (vertical == 0) {
      if (horizontal == 1) {
        l = std::min(l, box->x - box2->x - box2->w);
      } else if (horizontal == -1) {
        r = std::min(r, box2->x - box->x - box->w);
      }
    }
  }
  box->w += l + r - 2;
  box->x -= l - 1;
}

// As boxExpandLR
void boxExpandUD(BOX *box, BOXA *boxes) {
  int u = 99999, d = 99999;
  for (int j = 0; j < boxes->n; j++) {
    int horizontal = 0, vertical = 0;
    BOX *box2 = boxes->box[j];
    boxAlignment(box2, box, 0, &horizontal, &vertical);
    if (horizontal == 0) {
      if (vertical == -1) {
        u = std::min(u, box->y - box2->y - box2->h);
      } else if (vertical == 1) {
        d = std::min(d, box2->y - box->y - box->h);
      }
    }
  }
  box->h += u + d - 2;
  box->y -= u - 1;
}

double scoreBox(BOX *region, FigureType type, BOXA *bodyText,
                BOXA *graphicsBoxes, BOXA *claimedImages, PIX *original) {
  if (region->w < 25 or region->h < 25) {
    return 0;
  }
  int zero;
  pixZero(pixClipRectangle(original, region, NULL), &zero);
  if (zero) {
    return 0;
  }
  BOXA *b = boxaIntersectsBox(bodyText, region);
  l_int32 intersect = b->n;
  if (intersect > 0) {
    return 0;
  }
  for (int i = 0; i < claimedImages->n; i++) {
    // TODO remove this hack
    if (claimedImages->box[i] == region)
      continue;
    float psame;
    boxOverlapFraction(region, claimedImages->box[i], &psame);
    if (psame > 0.1) {
      return false;
    }
  }

  double score = 10;

  int largest = 0;
  bool lineAcross = false;
  b = boxaIntersectsBox(graphicsBoxes, region);
  for (int i = 0; i < b->n; i++) {
    float psame;
    boxOverlapFraction(region, b->box[i], &psame);
    if (psame > 0.98) {
      largest = std::max(largest, (b->box[i]->w) * (b->box[i]->h));
      if (b->box[i]->w / ((double)region->w) > 0.50) {
        lineAcross = true;
      }
    } else if (((b->box[i]->w * b->box[i]->h) > 1000 and psame < 0.50) or
               ((b->box[i]->w * b->box[i]->h) > 3000 and psame < 0.80)) {
      return 0;
    }
  }

  // Large graphical elements provide a strong bias, smaller or multiple
  // elements provide a lighter bias
  if (type == FIGURE) {
    if (largest > 18000) {
      score += 2;
    } else if (largest > 600 or b->n > 2) {
      score += 1;
    }
  } else if (lineAcross and largest > 1000) {
    score += 1;
  }
  return score +
         (region->w * region->h) / ((double)(original->w * original->h));
}

} // End namespace

std::vector<Figure> extractFigures(PIX *original, PageRegions &pageRegions,
                                   DocumentStatistics &docStats, bool verbose,
                                   bool showSteps,
                                   std::vector<Figure> &errors) {
  BOXA *bodytext = pageRegions.bodytext;
  BOXA *graphics = pageRegions.graphics;
  BOXA *captions = pageRegions.getCaptionsBoxa();
  std::vector<Caption> unassigned_captions = pageRegions.captions;
  int total_captions = captions->n;

  PIXA *steps = showSteps ? pixaCreate(4) : NULL;

  // Add bodyText boxes to fill up the margin
  BOX *margin;
  BOX *foreground;
  pixClipToForeground(original, NULL, &foreground);
  BOX *extent;
  boxaGetExtent(graphics, NULL, NULL, &extent);
  margin = boxBoundingRegion(extent, foreground);
  boxDestroy(&extent);
  boxaGetExtent(bodytext, NULL, NULL, &extent);
  margin = boxBoundingRegion(margin, extent);
  boxDestroy(&extent);
  boxaGetExtent(pageRegions.other, NULL, NULL, &extent);
  margin = boxBoundingRegion(margin, extent);
  int x = margin->x - 2, y = margin->y - 2, h = margin->h + 4,
      w = margin->w + 4;
  x = std::max(x, 0);
  y = std::max(y, 0);
  h = std::min((int)original->h, h);
  w = std::min((int)original->w, w);
  boxDestroy(&margin);
  boxaAddBox(bodytext, boxCreate(0, 0, original->w, y), L_CLONE);
  boxaAddBox(bodytext, boxCreate(0, y + h, original->w, original->h - y - h),
             L_CLONE);
  boxaAddBox(bodytext, boxCreate(0, 0, x, original->h), L_CLONE);
  boxaAddBox(bodytext, boxCreate(x + w, 0, original->w - x - w, original->h),
             L_CLONE);

  // Add captions to body text
  boxaJoin(bodytext, captions, 0, captions->n);

  if (showSteps)
    pixaAddPix(steps, original, L_CLONE);

  // Generate proposed regions for each caption box
  double center = original->w / 2.0;
  BOXAA *allProposals = boxaaCreate(captions->n);
  BOXA *claimedImages = boxaCreate(captions->n);
  for (int i = 0; i < captions->n; i++) {
    BOX *captBox = boxaGetBox(captions, i, L_CLONE);
    BOXA *proposals = boxaCreate(4);
    for (int j = 0; j < bodytext->n; j++) {
      BOX *txtBox = boxaGetBox(bodytext, j, L_CLONE);
      BOX *proposal = NULL;
      int tolerance = 2;
      int horizontal = 0;
      int vertical = 0;

      boxAlignment(captBox, txtBox, tolerance, &horizontal, &vertical);
      if (vertical * horizontal != 0 or (vertical == 0 and horizontal == 0)) {
        continue;
      }

      if (vertical == 0) {
        if (horizontal == 1) {
          proposal = boxRelocateOneSide(NULL, captBox,
                                        txtBox->x + txtBox->w + 2, L_FROM_LEFT);
        } else if (horizontal == -1) {
          proposal =
              boxRelocateOneSide(NULL, captBox, txtBox->x - 2, L_FROM_RIGHT);
        }
        boxExpandUD(proposal, bodytext);
        if (horizontal == -1) {
          proposal->w -= captBox->w + 1;
          proposal->x = captBox->x + captBox->w + 1;
        } else if (horizontal == 1) {
          proposal->w -= captBox->w + 1;
        }
      } else {
        if (vertical == 1) {
          proposal = boxRelocateOneSide(NULL, captBox,
                                        txtBox->y + txtBox->h + 3, L_FROM_TOP);
        } else if (vertical == -1) {
          proposal =
              boxRelocateOneSide(NULL, captBox, txtBox->y - 3, L_FROM_BOT);
        }
        boxExpandLR(proposal, bodytext);
        if (vertical == -1) {
          proposal->h -= captBox->h + 1;
          proposal->y = captBox->y + captBox->h + 1;
        } else if (vertical == 1) {
          proposal->h -= captBox->h + 1;
        }
      }

      // For two columns document, captions that do not
      // cross the center should not have regions pass the center
      if (docStats.documentIsTwoColumn()) {
        if (captBox->x + captBox->w <= center and
            proposal->x + proposal->w > center) {
          boxRelocateOneSide(proposal, proposal, center - 1, L_FROM_RIGHT);
        } else if (captBox->x >= center and proposal->x < center) {
          boxRelocateOneSide(proposal, proposal, center + 1, L_FROM_LEFT);
        }
      }

      BOX *clippedProposal;
      pixClipBoxToForeground(original, proposal, NULL, &clippedProposal);
      if (clippedProposal != NULL and
          scoreBox(clippedProposal, pageRegions.captions.at(i).type, bodytext,
                   graphics, claimedImages, original) > 0) {
        boxaAddBox(proposals, clippedProposal, L_CLONE);
      }
    }

    if (proposals->n > 0) {
      boxaaAddBoxa(allProposals, proposals, L_CLONE);
    } else {
      // Give up on this caption
      int on_caption = i - (total_captions - unassigned_captions.size());
      errors.push_back(Figure(unassigned_captions.at(on_caption), NULL));
      unassigned_captions.erase(unassigned_captions.begin() + on_caption);
    }
  }
  std::vector<Figure> figures = std::vector<Figure>();
  if (unassigned_captions.size() == 0) {
    return figures;
  }

  // Now go through every possible assignment of captions
  // to proposals pick the highest scorign one
  int numConfigurations = 1;
  for (int i = 0; i < allProposals->n; ++i) {
    numConfigurations *= allProposals->boxa[i]->n;
  }

  if (verbose)
    printf("Found %d possible configurations\n", numConfigurations);

  BOXA *bestProposals = NULL;
  std::vector<bool> bestKeep;
  int bestFound = -1;
  double bestScore = -1;
  for (int onConfig = 0; onConfig < numConfigurations; ++onConfig) {

    // Gather the proposed regions based on the configuration number
    int configNum = onConfig;
    BOXA *proposals = boxaCreate(allProposals->n);
    std::vector<bool> keep;
    for (int i = 0; i < allProposals->n; ++i) {
      int numProposals = allProposals->boxa[i]->n;
      int selected = configNum % numProposals;
      configNum = configNum / numProposals;
      boxaAddBox(proposals, allProposals->boxa[i]->box[selected], L_COPY);
    }

    // Attempt to split any overlapping regions
    for (int i = 0; i < proposals->n; ++i) {
      for (int j = i; j < proposals->n; ++j) {
        BOX *p1 = proposals->box[i];
        BOX *p2 = proposals->box[j];
        int eq;
        boxEqual(p1, p2, &eq);
        if (not eq)
          continue;
        int vertical, horizontal;
        boxAlignment(unassigned_captions.at(i).boundingBox,
                     unassigned_captions.at(j).boundingBox, 2, &horizontal,
                     &vertical);
        if (vertical == 0 or horizontal != 0)
          continue;

        double split = splitBoxVertical(original, p1);
        if (split > 0) {
          BOX *topClipped;
          BOX *botClipped;
          BOX *top = boxRelocateOneSide(NULL, p1, split - 1, L_FROM_BOT);
          pixClipBoxToForeground(original, top, NULL, &topClipped);
          BOX *bot = boxRelocateOneSide(NULL, p1, split + 1, L_FROM_TOP);
          pixClipBoxToForeground(original, bot, NULL, &botClipped);
          if (vertical == -1) {
            proposals->box[i] = topClipped;
            proposals->box[j] = botClipped;
          } else {
            proposals->box[i] = botClipped;
            proposals->box[j] = topClipped;
          }
          if (verbose)
            printf("Split a region vertically\n");
        }
      }
    }

    if (showSteps) {
      pixaAddPix(steps, pixDrawBoxa(original, proposals, 4, 0xff000000),
                 L_CLONE);
    }

    // Score the proposals
    int numFound = 0;
    double totalScore = 0;
    for (int i = 0; i < proposals->n; ++i) {
      double score =
          scoreBox(proposals->box[i], pageRegions.captions.at(i).type, bodytext,
                   graphics, proposals, original);
      totalScore += score;
      if (score > 0) {
        numFound += 1;
        keep.push_back(true);
      } else {
        keep.push_back(false);
      }
    }

    // Switch in for the current best needed
    if (numFound > bestFound or
        (numFound == bestFound and totalScore > bestScore)) {
      bestFound = numFound;
      bestScore = totalScore;
      bestProposals = proposals;
      bestKeep = keep;
    }
  }

  if (showSteps) {
    BOX *clip;
    PIXA *show = pixaCreate(4);
    pixClipBoxToForeground(original, NULL, NULL, &clip);
    int pad = 10;
    clip->x -= 10;
    clip->y -= 10;
    clip->w += pad * 2;
    clip->h += pad * 2;
    for (int i = 0; i < steps->n; ++i) {
      pixaAddPix(show, pixClipRectangle(steps->pix[i], clip, NULL), L_CLONE);
    }
    pixDisplay(pixaDisplayTiled(pixaConvertTo32(show), 4000, 1, 30), 0, 0);
  }

  for (int i = 0; i < bestProposals->n; ++i) {
    if (bestKeep.at(i)) {
      BOX *imageBox = bestProposals->box[i];
      int pad = 2;
      imageBox->x -= pad;
      imageBox->y -= pad;
      imageBox->w += pad * 2;
      imageBox->h += pad * 2;
      figures.push_back(Figure(unassigned_captions.at(i), imageBox));
    } else {
      errors.push_back(Figure(unassigned_captions.at(i), NULL));
    }
  }
  return figures;
}

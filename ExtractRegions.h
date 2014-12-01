#ifndef __figureextractor__ExtractRegions__
#define __figureextractor__ExtractRegions__

#include <vector>
#include <unordered_map>

#include <poppler/TextOutputDev.h>
#include <leptonica/allheaders.h>

#include "PDFUtils.h"
#include "ExtractCaptions.h"

/**
 Module for dividing up an image of a pdf file into
 different regions and classifying those regions.
 */

class PageRegions {

public:
  std::vector<Caption> captions; // Captions in the page
  BOXA *bodytext;                // Regions of body text,
  BOXA *graphics;                // Regions of graphics, should be high recall
  BOXA *other; // Any regions that have content were not catagorized either way

  PageRegions(std::vector<Caption> captions, BOXA *bodytext, BOXA *graphics,
              BOXA *other)
      : captions(captions), bodytext(bodytext), graphics(graphics),
        other(other) {}

  PIX *drawRegions(PIX *background);

  PIX *drawRegionsBW(PIX *background);

  BOXA *getCaptionsBoxa();
};

/**
   Given an image of a PDF page (orginal) the text (text) a PIX contain
   only the grpahics (graphics) and the locations of the caption starts
   of the page (captionStarts), returns a PageRegions object that breaks the
   PDF page in text, graphic, caption, or other regions. Does not take
   ownerish of any arguements.
 */
PageRegions getPageRegions(PIX *original, TextPage *text, PIX *graphics,
                           const std::vector<Caption> &captionStarts,
                           DocumentStatistics &docStats, int page, bool verbose,
                           bool showSteps, std::vector<Figure> &errors);

#endif /* defined(__figureextractor__ExtractRegions__) */

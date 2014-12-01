#ifndef __figureextactor__ExtractFigures__
#define __figureextactor__ExtractFigures__

#include <vector>

#include <goo/GooList.h>
#include <leptonica/allheaders.h>

#include "ExtractRegions.h"
#include "PDFUtils.h"

/*
  Given an image of a PDF page (original) and the regions of that page
  (PageRegions)
  return a vector of Figure objects of the page.
 */
std::vector<Figure> extractFigures(PIX *original, PageRegions &pageRegions,
                                   DocumentStatistics &docStats, bool verbose,
                                   bool showSteps, std::vector<Figure> &errors);

#endif /* defined(__figureextactor__ExtractFigures__) */

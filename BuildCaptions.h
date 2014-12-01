#ifndef __figureextactor__BuildCaptions__
#define __figureextactor__BuildCaptions__

#include <vector>

#include "TextUtils.h"
#include "PDFUtils.h"

/**
   Builds a list of captions constructed from a list of caption starts.
 */
std::vector<Caption> buildCaptions(std::vector<CaptionStart> &starts,
                                   DocumentStatistics &docStats, TextPage *text,
                                   PIX *graphics, int verbose);

#endif /* defined(__figureextactor__BuildCaptions__) */

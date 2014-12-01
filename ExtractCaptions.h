#ifndef __figureextractor__ExtractCaptions__
#define __figureextractor__ExtractCaptions__

#include <vector>
#include <map>

#include <TextOutputDev.h>

#include "TextUtils.h"
#include "PDFUtils.h"

/*
 * Returns a map of page number -> CaptionStarts that occur on that page. No two
 * captions will be of the same number and type, but numbers for each type might
 *not be
 * consecutive if some captions could not be located. All returned captions
 * are expected be valid. If it appears the Figures exists, but the caption
 * could not be located, the partially built Figures are pushed onto errors.
 **/
std::map<int, std::vector<CaptionStart>>
extractCaptionsFromText(const std::vector<TextPage *> &textPages, bool verbose,
                        std::vector<Figure> &errors);

#endif /* defined(__figureextractor__ExtractCaptions__) */

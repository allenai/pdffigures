#ifndef __figureextractor__TextUtils__
#define __figureextractor__TextUtils__

#include <vector>
#include <unordered_map>
#include <string>

#include <TextOutputDev.h>
#include <Page.h>
#include "PDFUtils.h"

// Class to track document level statistics
class DocumentStatistics {
public:
  DocumentStatistics(std::vector<TextPage *> &textPages, PDFDoc *doc,
                     bool quiet);
  double getModeFont();

  bool wordIsLarge(TextWord *word);

  int lineIsAligned(double x, double x2);

  bool wordIsStandardFont(TextWord *word);

  int lineIsAlignedToTol(double x, double x2, double l_tol, double r_tol);

  bool isPageHeader(TextLine *line);

  bool isPageNumber(TextLine *line);

  bool documentIsTwoColumn();

  bool lineIsBold(TextLine *line);

  bool isBoldCentered(double x, double x2);

  bool isBodyTextGraphical();

private:
  int totalWords;
  int totalLines;
  double lMarginFirst;
  double lMarginSecond;
  double rMarginFirst;
  double rMarginSecond;
  double modeFont;
  std::string modeFontName;

  bool twoColumn;
  bool rightAligned;
  bool hasPageNumbers;
  bool imageFilled;

  std::unordered_map<int, int> boldCentersUp;
  std::unordered_map<int, int> boldCentersDown;
  std::unordered_map<std::string, int> pageHeaders;
  std::unordered_map<double, int> rMarginCounts;
  std::unordered_map<double, int> lMarginCounts;
};

// Debugging
void printTextProperties(TextPage *page, DocumentStatistics *docStats,
                         bool onlyLineStarts);

std::vector<TextLine *> getLines(TextPage *textPage);

void getTextLineBB(TextLine *line, double *minX, double *minY, double *maxX,
                   double *maxY);

bool wordIsItalic(TextWord *const word);

bool wordIsBold(TextWord *const word);

bool wordEndsWithPeriod(TextWord *const word);

#endif

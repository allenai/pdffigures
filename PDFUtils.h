#ifndef __figureextractor__PDFUtils__
#define __figureextractor__PDFUtils__

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <memory>

#include <TextOutputDev.h>

#include <leptonica/allheaders.h>

enum FigureType { FIGURE, TABLE };

const char *getFigureTypeString(FigureType type);

class CaptionStart {
public:
  CaptionStart(int page, int number, TextWord *word, FigureType type);

  int page;
  FigureType type;
  int number;

  // First word of the caption, i.e. 'Figure'
  TextWord *word;
};

class Caption {
public:
  Caption(CaptionStart captionStart, BOX *boundingBox);

  Caption(int page, int number, FigureType type, BOX *boundingBox);

  int page;
  int number;
  FigureType type;
  BOX *boundingBox;
};

class Figure {
public:
  Figure(Caption caption, BOX *imageBB);

  Figure(CaptionStart captionStart);

  FigureType type;
  int page;
  int number;
  BOX *imageBB;
  BOX *captionBB;
};

// Draws a box on a pix, and displays is. Useful for debugging.
void displayBox(PIX *pix, BOX *box);

/*
  Returns true iff given page of the document as an image with
  a bounding box that contains the entire page. In practice
  this can occur even if the image small so this method is
  unreliable.
**/
bool isFilledByImage(PDFDoc *doc, int page);

// Gets a PIX of the given page rendered at the given dpi.
std::unique_ptr<PIX> getFullRenderPix(PDFDoc *doc, int page, double dpi);

// Gets a PIX of the given page rendered wthout text at the given dpi.
std::unique_ptr<PIX> getGraphicOnlyPix(PDFDoc *doc, int page, double dpi);

// Gets a PIX of the given page rendered at the given dpi with splashModeRGB8 color mode.
std::unique_ptr<PIX> getFullColorRenderPix(PDFDoc *doc, int page, double dpi);

// Gets the TextPage* objects of a document at a given dpi.
std::vector<TextPage *> getTextPages(PDFDoc *doc, double dpi);

// Get a PIX with the given figures drawn on it
PIX *drawFigureRegions(PIX *background, const std::vector<Figure> &figures);

// Return a new GooString with new lines and control characters removed and
// with JSON illegal characters escaped.
GooString *jsonSanitizeUTF8(GooString *str);

void writeText(TextPage *page, BOX *bb, const char *name, std::ostream &output);

void saveFiguresImage(std::vector<Figure> &figures, PIX *original,
                      std::string prefix);

void saveFiguresFullColorImage(std::vector<Figure> &figures, PIX *original,
                      std::string prefix, int multidpi);

void writeFigureJSON(Figure &figures, int height, int width, double dpi,
                     std::vector<TextPage *> &text, std::ostream &output);

#endif /* defined(__figureextractor__PDFUtils__) */

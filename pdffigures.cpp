#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>

#include <PDFDocFactory.h>
#include <GlobalParams.h>
#include <getopt.h>

#include "ExtractCaptions.h"
#include "BuildCaptions.h"
#include "PDFUtils.h"
#include "ExtractRegions.h"
#include "ExtractFigures.h"

const std::string version = "1.0.6";

void printUsage() {
  printf("Usage: figureextractor [flags] </path/to/pdf>\n");
  printf("--version\n");
  printf("-v, --verbose\n");
  printf(
      "-m, --save-mistakes: If combined with -f,-o,-a additionally save/show figures that were detected\
 but could not be extracted successfully\n");
  printf("-s, --show-steps: Display image processing steps\n");
  printf("-f, --show-final: Display pages with captions and images marked\n");
  printf("-a, --save-final <prefix: Save page images with captions and images "
         "marked to the given prefix. Files will be saved to "
         "prefix-<page#>.png\n");
  printf("-o, --save-figures <prefix>: Save images of detected figures to "
         "prefix. Files are save to prefix-<(Table|Figure)>-<Number>.png\n");
  printf("-c, --save-color-images: Additionally save color images for figures and tables."
    "Files are save to prefix-<(Table|Figure)>-c<Number>.png\n");
  printf("-j, --save-json <prefix>: Save json encoding of detected figures to "
         "prefix. Files are save to prefix.json\n");
  printf("-r, --reverse: Go through pages in reverse order\n");
  printf("-p, --page <page#>: Run only for the given page\n");
  printf("-i, --text-as-image: Attempt to parse documents even if the "
         "document's text is encoded as part of an embedded image (usually "
         "caused by scanned documents that have been processed with OCR). "
         "These documents are not handeled well at the moment so precision is "
         "liable to be poor\n");
  printf("-h, --help show usage\n");
}

int main(int argc, char **argv) {
  int verbose = false;
  int showSteps = false;
  int showFinal = false;
  int onlyPage = -1;
  int reverse = false;
  int saveMistakes = false;
  int textAsImage = false;
  int saveColorImage = false;
  std::string imagePrefix = "";
  std::string jsonPrefix = "";
  std::string finalPrefix = "";
  const double resolution = 100;
  const int resMultiply = 4; 

  const struct option long_options[] = {
      {"version", no_argument, NULL, 0},
      {"verbose", no_argument, &verbose, true},
      {"show-steps", no_argument, &showSteps, true},
      {"show-final", no_argument, &showFinal, true},
      {"save-final", required_argument, NULL, 'a'},
      {"save-figures", required_argument, NULL, 'o'},
      {"save-color-images", no_argument, &saveColorImage, true},
      {"save-json", required_argument, NULL, 'j'},
      {"page", required_argument, NULL, 'p'},
      {"reverse", no_argument, &reverse, 'r'},
      {"text-as-image", no_argument, &textAsImage, true},
      {"save-mistakes", no_argument, &saveMistakes, true},
      {"help", no_argument, NULL, 'h'},
      {0, 0, 0, 0}};

  int opt;
  int optionIndex;
  while ((opt = getopt_long(argc, argv, "svfrmicj:a:o:p:", long_options,
                            &optionIndex)) != -1) {
    switch (opt) {
    case 0:
      if (optionIndex == 0) {
        printf("pdffigures version %s\n", version.c_str());
        return 0;
      }
      break; // flag was set
    case 'm':
      saveMistakes = true;
      break;
    case 'v':
      verbose = true;
      break;
    case 's':
      showSteps = true;
      break;
    case 'f':
      showFinal = true;
      break;
    case 'p':
      onlyPage = std::stoi(optarg);
      break;
    case 'o':
      imagePrefix = optarg;
      break;
    case 'c':
      saveColorImage = true;
      break;
    case 'a':
      finalPrefix = optarg;
      break;
    case 'j':
      jsonPrefix = optarg;
      break;
    case 'i':
      textAsImage = true;
      break;
    case 'h':
      printUsage();
      return 0;
    case '?':
      printUsage();
      return 1;
    }
  }

  if (optind > argc - 1) {
    printf("No PDF file given!\n");
    printUsage();
    return 1;
  }

  if (optind < argc - 1) {
    printf("Extra argument given\n");
    printUsage();
    return 1;
  }

  if (not showFinal and not showSteps and finalPrefix.length() == 0 and
      not verbose and imagePrefix.length() == 0 and jsonPrefix.length() == 0) {
    printf("No output requested\n");
    printUsage();
    return 1;
  }

  globalParams = new GlobalParams(); // Set up poppler
  // Build a writable str to pass to setTextEncoding
  std::string str = "UTF-8";
  std::vector<char> writableStr(str.begin(), str.end());
  writableStr.push_back('\0');
  globalParams->setTextEncoding(&writableStr.at(0));

  std::unique_ptr<PDFDoc> doc(
      PDFDocFactory().createPDFDoc(GooString(argv[optind]), NULL, NULL));
  if (not doc->isOk()) {
    return 1;
  }

  std::vector<TextPage *> pages = getTextPages(doc.get(), resolution);

  if (verbose)
    printf("Scanned %d pages\n", (int)pages.size());
  DocumentStatistics docStats = DocumentStatistics(pages, doc.get(), verbose);

  if (docStats.isBodyTextGraphical() and not textAsImage) {
    printf("Body text appears to be encoded as graphics, skipping (use -i to "
           "parse these kinds of documents)\n");
    return 0;
  }

  std::vector<Figure> errors = std::vector<Figure>();

  std::map<int, std::vector<CaptionStart>> captionStarts =
      extractCaptionsFromText(pages, verbose);

  if (captionStarts.size() == 0) {
    printf("No captions found!");
    if (jsonPrefix.length() != 0) {
      // To be consistent, output a JSON file anyway
      std::ofstream output((jsonPrefix + ".json").c_str());
      output << "[]";
      output.close();
    }
    return 0;
  }

  if (verbose) {
    for (auto &c : captionStarts) {
      printf("\nPage %d:", c.first);
      for (size_t i = 0; i < c.second.size(); ++i) {
        printf(" %s%d", getFigureTypeString(c.second.at(i).type),
               c.second.at(i).number);
      }
    }
    printf("\n");
  }

  std::vector<Figure> allFigures;
  std::map<int, std::pair<int, int>> pageSizes;
  std::map<int, std::vector<CaptionStart>>::iterator start =
      captionStarts.begin();
  std::map<int, std::vector<CaptionStart>>::iterator end = captionStarts.end();
  while (start != end) {
    int onPage;
    if (reverse) {
      end--;
      onPage = end->first;
    } else {
      onPage = start->first;
      start++;
    }
    if (onlyPage >= 0 and onlyPage != onPage)
      continue;
    if (verbose)
      printf("Working on page %d\n", onPage);

    std::unique_ptr<PIX> fullRender =
        getFullRenderPix(doc.get(), onPage + 1, resolution);
    std::unique_ptr<PIX> fullRender1d(pixConvertTo1(fullRender.get(), 250));

    std::unique_ptr<PIX> graphics1d;
    if (not docStats.isBodyTextGraphical()) {
      std::unique_ptr<PIX> graphics =
          getGraphicOnlyPix(doc.get(), onPage + 1, resolution);
      graphics1d = std::unique_ptr<PIX>(pixConvertTo1(graphics.get(), 250));
    } else {
      graphics1d = std::unique_ptr<PIX>(pixCreateTemplate(fullRender1d.get()));
    }

    std::unique_ptr<PIX> fullColorRender =
        getFullColorRenderPix(doc.get(), onPage + 1, resolution * resMultiply);

    // Remove graphical elements that did not show up in the original due
    // to PDF shenanigans.
    pixAnd(graphics1d.get(), graphics1d.get(), fullRender1d.get());

    std::vector<Caption> captions =
        buildCaptions(captionStarts.at(onPage), docStats, pages.at(onPage),
                      graphics1d.get(), verbose);
    PageRegions regions =
        getPageRegions(fullRender1d.get(), pages.at(onPage), graphics1d.get(),
                       captions, docStats, onPage, verbose, showSteps, errors);
    std::vector<Figure> figures;
    if (regions.captions.size() == 0) {
      figures = std::vector<Figure>();
    } else {
      figures = extractFigures(fullRender1d.get(), regions, docStats, verbose,
                               showSteps, errors);
    }

    if (figures.size() == 0 and verbose) {
      printf("Warning: No figures recovered");
    }

    if (saveMistakes) {
      for (Figure &fig : errors) {
        figures.push_back(fig);
      }
    }

    if (jsonPrefix.length() != 0) {
      for (Figure &fig : figures) {
        allFigures.push_back(fig);
      }
      pageSizes[onPage] = std::pair<int, int>(fullRender->w, fullRender->h);
    }
    if (imagePrefix.length() != 0) {
      saveFiguresImage(figures, fullRender.get(), imagePrefix);
    }
    if (saveColorImage && imagePrefix.length() != 0) {
      saveFiguresFullColorImage(figures, fullColorRender.get(), imagePrefix, resMultiply);
    }
    if (showFinal or finalPrefix.length() != 0) {
      std::unique_ptr<PIX> final(drawFigureRegions(fullRender.get(), figures));
      if (showFinal)
        pixDisplay(final.get(), 0, 0);
      if (finalPrefix.length() > 0)
        pixWriteImpliedFormat(
            (finalPrefix + "-" + std::to_string(onPage) + ".png").c_str(),
            final.get(), 0, 0);
    }
    errors.clear();
    if (verbose)
      printf("Done\n\n");
  }

  if (jsonPrefix.length() != 0) {
    std::ofstream output((jsonPrefix + ".json").c_str());
    output << "[\n";
    for (size_t i = 0; i < allFigures.size(); ++i) {
      Figure fig = allFigures[i];
      int width, height;
      if (fig.page != -1) {
        width = pageSizes[fig.page].first;
        height = pageSizes[fig.page].second;
      } else {
        width = -1;
        height = -1;
      }
      writeFigureJSON(allFigures[i], width, height, resolution, pages, output);
      if (i != allFigures.size() - 1) {
        output << ",";
      }
      output << "\n";
    }
    output << "]\n";
    output.close();
    if (verbose) {
      printf("Saved %d figures to %s\n", (int)allFigures.size(),
             (jsonPrefix + ".json").c_str());
    }
  }
  for (auto &textPage : pages) {
    textPage->decRefCnt();
  }
}

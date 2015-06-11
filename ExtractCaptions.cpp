#include <unordered_map>
#include <regex>

#include "ExtractCaptions.h"

namespace {

// ****** Gathing caption candidates *******
// TODO using an int as the FigureId is kind of a hack
typedef int FigureId;

class CaptionCandidate {
public:
  CaptionCandidate()
      : word(NULL), lineStart(false), type(FIGURE), number(-1), page(-1),
        periodMatch(false), colonMatch(false) {}

  CaptionCandidate(TextWord *word, bool lineStart, FigureType type, int number,
                   int page, bool periodMatch, bool colonMatch)
      : word(word), lineStart(lineStart), type(type), number(number),
        page(page), periodMatch(periodMatch), colonMatch(colonMatch) {}

  FigureId getId() { return number * (type == FIGURE ? 1 : -1); }

  TextWord *word;
  bool lineStart;
  FigureType type;
  int number;
  int page;
  bool periodMatch;
  bool colonMatch;
};

CaptionCandidate constructCandidate(TextWord *word, int page, bool lineStart) {
  if (word->getNext() == NULL)
    return CaptionCandidate();

  const std::regex wordRegex = std::regex("^(Figure|Fig|FIG|Table)$");
  std::match_results<const char *> wordMatch;
  if (not std::regex_match(word->getText()->getCString(), wordMatch, wordRegex))
    return CaptionCandidate();

  const std::regex numberRegex = std::regex("^([0-9]+)(:|\\.)?$");

  std::match_results<const char *> numberMatch;
  std::regex_match(word->getNext()->getText()->getCString(), numberMatch,
                   numberRegex);

  int number;
  std::string captionNumStr;
  if (not numberMatch.empty()) {
    captionNumStr = numberMatch[0].str();
    number = std::stoi(numberMatch[1].str());
  } else {
    return CaptionCandidate();
  }
  bool periodMatch = false, colonMatch = false;
  if (captionNumStr.at(captionNumStr.length() - 1) == ':') {
    colonMatch = true;
  } else {
    periodMatch = true;
  }
  FigureType type = wordMatch[0].str().at(0) == 'T' ? TABLE : FIGURE;
  return CaptionCandidate(word, lineStart, type, number, page, periodMatch,
                          colonMatch);
}

// Maps ids -> all candidates that have that id
typedef std::unordered_map<FigureId,
                           std::unique_ptr<std::vector<CaptionCandidate>>>
    CandidateCollection;

CandidateCollection collectCandidates(const std::vector<TextPage *> &pages) {
  CandidateCollection collection = CandidateCollection();
  for (size_t i = 0; i < pages.size(); ++i) {
    TextFlow *flow = pages.at(i)->getFlows();
    while (flow != NULL) {
      TextBlock *block = flow->getBlocks();
      while (block != NULL) {
        TextLine *line = block->getLines();
        while (line != NULL) {
          TextWord *word = line->getWords();
          bool lineStart = true;
          while (word != NULL) {
            CaptionCandidate cc = constructCandidate(word, i, lineStart);
            if (cc.word != NULL) {
              int id = cc.getId();
              if (collection.find(id) == collection.end()) {
                collection[id] = std::unique_ptr<std::vector<CaptionCandidate>>(
                    new std::vector<CaptionCandidate>());
              }
              collection[cc.getId()]->push_back(cc);
            }
            word = word->getNext();
            lineStart = false;
          }
          line = line->getNext();
        }
        block = block->getNext();
      }
      flow = flow->getNext();
    }
  }
  return collection;
}

// ****** CaptionCandidate Filteres ******

class CandidateFilter {
public:
  CandidateFilter(const char *name, const bool asGroup)
      : name(name), asGroup(asGroup) {}

  const char *name;
  const bool asGroup;
  virtual bool check(const CaptionCandidate &cc) = 0;
};

class ColonOnly : public CandidateFilter {
public:
  ColonOnly() : CandidateFilter("Colon Only", true) {}
  bool check(const CaptionCandidate &cc) { return cc.colonMatch; }
};

class PeriodOnly : public CandidateFilter {
public:
  PeriodOnly() : CandidateFilter("Period Only", true) {}
  bool check(const CaptionCandidate &cc) { return cc.periodMatch; }
};

class LineStartOnly : public CandidateFilter {
public:
  LineStartOnly() : CandidateFilter("Line Start Only", false) {}
  bool check(const CaptionCandidate &cc) { return cc.lineStart; }
};

class BoldOnly : public CandidateFilter {
public:
  BoldOnly() : CandidateFilter("Bold Only", true) {}
  bool check(const CaptionCandidate &cc) { return wordIsBold(cc.word); }
};

class ItalicOnly : public CandidateFilter {
public:
  ItalicOnly() : CandidateFilter("Italic Only", true) {}
  bool check(const CaptionCandidate &cc) { return wordIsItalic(cc.word); }
};

class NextWordOnly : public CandidateFilter {
public:
  NextWordOnly() : CandidateFilter("Next Word Only", false) {}
  bool check(const CaptionCandidate &cc) {
    return cc.word->getNext()->getNext() != NULL;
  }
};

class FontSizeChangeOnly : public CandidateFilter {
public:
  FontSizeChangeOnly() : CandidateFilter("Font Size Change Only", true) {}
  bool check(const CaptionCandidate &cc) {
    TextWord *next = cc.word->getNext()->getNext();
    if (next == NULL)
      return false;
    return next->getFontSize() != cc.word->getFontSize();
  }
};

class FontChangeOnly : public CandidateFilter {
public:
  FontChangeOnly() : CandidateFilter("Font Change Only", true) {}
  bool check(const CaptionCandidate &cc) {
    TextWord *next = cc.word->getNext()->getNext();
    if (next == NULL)
      return false;
    GooString *nextFontName =
        next->getFontInfo(next->getLength() - 1)->getFontName();
    GooString *fontName =
        cc.word->getFontInfo(cc.word->getLength() - 1)->getFontName();
    return fontName->cmp(nextFontName) != 0;
  }
};

// ********** Applying the filters to get the final results ******

// TODO we should be a bit more clever and accept filter that remove
// caption reference if it is the only way to remove all duplicates
bool applyFilter(CandidateFilter *filter, CandidateCollection &collection) {
  bool removedAnything = false;
  for (auto &ccs : collection) {
    size_t filterCount = 0;
    for (CaptionCandidate cc : *ccs.second.get()) {
      if (filter->check(cc))
        filterCount += 1;
    }
    if (filterCount == 0 and filter->asGroup) {
      return false;
    } else if (filterCount < ccs.second->size() and filterCount >= 1) {
      removedAnything = true;
    }
  }
  if (not removedAnything)
    return false;
  for (auto &ccs : collection) {
    for (size_t i = 0; i < ccs.second.get()->size(); ++i) {
      if (ccs.second->size() > 1 and not filter->check(ccs.second->at(i))) {
        ccs.second->erase(ccs.second->begin() + i);
        --i;
      }
    }
  }
  return true;
}

bool anyDuplicates(const CandidateCollection &collection) {
  for (auto &ccs : collection) {
    if (ccs.second.get()->size() > 1) {
      return true;
    }
  }
  return false;
}

} // End namespace

std::map<int, std::vector<CaptionStart>>
extractCaptionsFromText(const std::vector<TextPage *> &textPages,
                        bool verbose) {
  CandidateCollection candidates = collectCandidates(textPages);
  // In order to be considered
  ColonOnly f1 = ColonOnly();
  PeriodOnly f2 = PeriodOnly();
  BoldOnly f3 = BoldOnly();
  ItalicOnly f4 = ItalicOnly();
  FontSizeChangeOnly f5 = FontSizeChangeOnly();
  FontChangeOnly f6 = FontChangeOnly();
  LineStartOnly f7 = LineStartOnly();
  NextWordOnly f8 = NextWordOnly();
  std::vector<CandidateFilter *> filters = {&f1, &f2, &f3, &f4,
                                            &f5, &f6, &f7, &f8};

  if (verbose) {
    printf("Scanning for captions...\n");
    int nCandidates = 0;
    for (auto &ccs : candidates) {
      nCandidates += ccs.second->size();
    }
    printf("Collected %d candidates for %d detected captions\n", nCandidates,
           (int)candidates.size());
  }

  bool triedAll = false;
  while (anyDuplicates(candidates) and not triedAll) {
    triedAll = true;
    for (CandidateFilter *cf : filters) {
      if (applyFilter(cf, candidates)) {
        if (verbose) {
          int nCandidates = 0;
          for (auto &ccs : candidates) {
            nCandidates += ccs.second->size();
          }
          printf("Applied filter %s (%d remain)\n", cf->name, nCandidates);
        }
        triedAll = false;
        break;
      }
    }
  }

  // Check for non consecutive figures / tables, add any found as errors
  if (verbose) {
    int maxTable = 0;
    int maxFigure = 0;
    int nTables = 0;
    int nFigures = 0;
    for (auto &ccs : candidates) {
      if (ccs.first > 0) {
        nFigures++;
        maxFigure = std::max(ccs.first, maxFigure);
      } else {
        nTables++;
        maxTable = std::max(-ccs.first, maxTable);
      }
    }
    if (maxTable != nTables) {
      printf("Warning: Max table number found was %d, but only found %d table "
             "captions!\n",
             maxTable, nTables);
    }
    if (maxFigure != nFigures) {
      printf(
          "Warning: Max figure number found was %d, but only found %d figure "
          "captions!\n",
          maxFigure, nFigures);
    }
  }

  std::map<int, std::vector<CaptionStart>> output =
      std::map<int, std::vector<CaptionStart>>();

  // Add in all the captions
  for (CandidateCollection::iterator it = candidates.begin();
       it != candidates.end();) {
    std::vector<CaptionCandidate> *captionOptions = it->second.get();
    if (captionOptions->size() == 1) {
      CaptionCandidate cc = captionOptions->at(0);
      output[cc.page].push_back(
          CaptionStart(cc.page, cc.number, cc.word, cc.type));
    } else if (captionOptions->size() > 1) {
      if (verbose) {
        printf("Unable to filter out duplicate candidates for %s%d\n",
               getFigureTypeString(captionOptions->at(0).type),
               captionOptions->at(0).number);
      }
    } else {
      throw std::runtime_error("Should never filter out all candidates!");
    }
    candidates.erase(it++);
  }

  if (verbose)
    printf("Done parsing captions.\n\n");
  return output;
}

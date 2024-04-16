#include "DirectiveRewriter.h"

#include "clang/AST/ParentMapContext.h"
#include "clang/Basic/SourceManager.h"

#include <boost/container/flat_set.hpp>

using namespace clang;

struct UpdateDirInfo {
  const Stmt *FullStmt;
  boost::container::flat_set<const ValueDecl *> Decls;

  UpdateDirInfo(const Stmt *FullStmt) : FullStmt(FullStmt) {}
};

struct ClauseDirInfo {
  const OMPExecutableDirective *Directive;
  boost::container::flat_set<const ValueDecl *> FirstPrivateDecls;

  ClauseDirInfo(const OMPExecutableDirective *Directive)
      : Directive(Directive) {}
};

/* Finds the outermost Stmt in ContainingStmt that captures S. Returns NULL on
 * error.
 */
const Stmt *getSemiTerminatedStmt(ASTContext &Context, const Stmt *S) {
  const Stmt *CurrentStmt = S;
  while (true) {
#if DEBUG_LEVEL >= 1
    CurrentStmt->printPretty(llvm::outs(), NULL, PrintingPolicy(LangOptions()));
    llvm::outs() << "end\n";
#endif
    const auto &ImmediateParents = Context.getParents(*CurrentStmt);
    if (ImmediateParents.size() == 0)
      return nullptr;

    const Stmt *ParentStmt = ImmediateParents[0].get<Stmt>();
    if (!ParentStmt) {
      const VarDecl *VD = ImmediateParents[0].get<VarDecl>();
      if (VD == nullptr) {
        // we may need a more specific check here. This was added because a for
        // loop didn't have a parent. So this works for now.
        return CurrentStmt;
      }
      const auto &VDParents = Context.getParents(*VD);
      return VDParents[0].get<Stmt>();
    }

    if (isa<CompoundStmt>(ParentStmt))
      return CurrentStmt;

    CurrentStmt = ParentStmt;
  }
}

/* Returns the SourceLocation immediately after a Semi-Terminated Stmt (or
 * closing bracket).
 */
SourceLocation getSemiTerminatedStmtEndLoc(SourceManager &SourceMgr,
                                           const Stmt *S) {
  SourceLocation Loc = S->getEndLoc();
  const char *Source = SourceMgr.getCharacterData(Loc);
  unsigned int Offset = 1;
  int OpenBrackets = 1;
  while (*Source != '\0' && *Source != ';' && OpenBrackets != 0) {
    if (*Source == '{')
      ++OpenBrackets;
    if (*Source == '}')
      --OpenBrackets;
    ++Offset;
    ++Source;
  }
  // fix off by one for closed brackets
  if (OpenBrackets == 0)
    --Offset;

  return Loc.getLocWithOffset(Offset);
}

bool isIndentChar(char C) {
  switch (C) {
  case ' ':
  case '\t':
    return true;
  default:
    return false;
  }
}

/* Returns the indentation of the line of the given SourceLocation.
 */
std::string getIndentation(SourceManager &SourceMgr, SourceLocation Loc) {
  std::string Indent;

  FileID FID = SourceMgr.getFileID(Loc);
  unsigned int Line = SourceMgr.getSpellingLineNumber(Loc);
  SourceLocation BeginLn = SourceMgr.translateLineCol(FID, Line, 1);

  const char *Source = SourceMgr.getCharacterData(BeginLn);
  SourceLocation FileEndLoc = SourceMgr.getLocForEndOfFile(FID);
  const char *FileEnd = SourceMgr.getCharacterData(FileEndLoc);

  while (Source != FileEnd + sizeof(char) && isIndentChar(*Source)) {
    Indent += *Source;
    ++Source;
  }
  return Indent;
}

/* Returns the whitespace representing a single level of indentation.
 */
std::string getBodyIndentation(SourceManager &SourceMgr,
                               const CompoundStmt *Body) {
  SourceLocation BeginLoc = Body->getBeginLoc().getLocWithOffset(1);
  const char *Source = SourceMgr.getCharacterData(BeginLoc);
  SourceLocation EndLoc = Body->getEndLoc();
  const char *End = SourceMgr.getCharacterData(EndLoc);
  std::string Indent;
  // Ignore all text on the same line as the opening bracket.
  while (Source != End + sizeof(char) && *Source != '\n') {
    ++Source;
  }
  // Count indentation of each line until one is found with text then return
  // that indentation.
  while (Source != End + sizeof(char) && isspace(*Source)) {
    Indent += *Source;
    if (*Source == '\n')
      Indent = "";

    ++Source;
  }
  return Indent;
}

/* Returns the whitespace representing a single level of indentation.
 */
std::string getIndentationStep(SourceManager &SourceMgr,
                               const FunctionDecl *FD) {
  std::string ParentIndent = getIndentation(SourceMgr, FD->getBeginLoc());
  const CompoundStmt *Body = dyn_cast<CompoundStmt>(FD->getBody());
  if (!Body)
    return ParentIndent;

  std::string BodyIndent = getBodyIndentation(SourceMgr, Body);

  return BodyIndent.substr(ParentIndent.length());
}

void increaseIndentation(Rewriter &R, const TargetDataRegion *Data,
                         const std::string &IndentStep) {
  SourceManager &SM = R.getSourceMgr();

  SourceLocation OpeningLoc = Data->getBeginLoc();
  SourceLocation ClosingLoc = Data->getEndLoc();
  FileID FID = SM.getFileID(OpeningLoc);
  unsigned int BeginLn = SM.getSpellingLineNumber(OpeningLoc);
  unsigned int EndLn = SM.getSpellingLineNumber(ClosingLoc);
  for (unsigned Ln = BeginLn + 1; Ln <= EndLn; ++Ln) {
    SourceLocation InsertLoc = SM.translateLineCol(FID, Ln, 1);
    R.InsertTextBefore(InsertLoc, IndentStep);
  }

  return;
}

void rewriteDataMap(Rewriter &R, ASTContext &Context,
                    const TargetDataRegion *Data,
                    const std::string &IndentStep) {
  SourceManager &SM = R.getSourceMgr();

  std::string MapDirective;
  if (Data->getKernels().size() != 1 ||
      Data->getKernels().front()->getBeginLoc() != Data->getBeginLoc()) {
    // create a new directive rather than add to an existing one
    MapDirective = "#pragma omp target data";
  }

  if (!Data->getMapAlloc().empty()) {
    MapDirective += " map(alloc:";
    for (const AccessInfo &Access : Data->getMapAlloc()) {
      MapDirective += Access.VD->getNameAsString() + ",";
    }
    MapDirective.back() = ')';
  }
  if (!Data->getMapTo().empty()) {
    MapDirective += " map(to:";
    for (const AccessInfo &Access : Data->getMapTo()) {
      MapDirective += Access.VD->getNameAsString() + ",";
    }
    MapDirective.back() = ')';
  }
  if (!Data->getMapFrom().empty()) {
    MapDirective += " map(from:";
    for (const AccessInfo &Access : Data->getMapFrom()) {
      MapDirective += Access.VD->getNameAsString() + ",";
    }
    MapDirective.back() = ')';
  }
  if (!Data->getMapToFrom().empty()) {
    MapDirective += " map(tofrom:";
    for (const AccessInfo &Access : Data->getMapToFrom()) {
      MapDirective += Access.VD->getNameAsString() + ",";
    }
    MapDirective.back() = ')';
  }

  if (MapDirective[0] != '#') {
    // Append the map directives to the end of the first and only kernel
    // spawning directive.
    R.InsertTextBefore(Data->getKernels().front()->getEndLoc(), MapDirective);
    return;
  }

  MapDirective += "\n";
  std::string Indent = getIndentation(SM, Data->getBeginLoc());
  MapDirective += Indent;
  MapDirective += "{\n";
  MapDirective += Indent + IndentStep;
  R.InsertTextBefore(Data->getBeginLoc(), MapDirective);

  std::string MapDirectiveClosing = "\n";
  MapDirectiveClosing += Indent;
  MapDirectiveClosing += "}\n";
  SourceLocation ClosingLoc = Data->getEndLoc().getLocWithOffset(1);
  // Accommodate for DoStmt not including it's semi.
  if (SM.getCharacterData(ClosingLoc)[0] == ';')
    ClosingLoc = ClosingLoc.getLocWithOffset(1);

  R.InsertTextAfter(ClosingLoc, MapDirectiveClosing);
  increaseIndentation(R, Data, IndentStep);
  return;
}

void rewriteUpdateTo(Rewriter &R, ASTContext &Context,
                     const TargetDataRegion *Data,
                     const std::string &IndentStep) {
  if (Data->getUpdateTo().empty())
    return;

  SourceManager &SM = R.getSourceMgr();

  std::vector<UpdateDirInfo> UpdateToList;
  for (const AccessInfo &Access : Data->getUpdateTo()) {
    const Stmt *FullStmt = getSemiTerminatedStmt(Context, Access.S);

    if (Access.Barrier != ScopeBarrier::LoopEnd) {
      if (const ForStmt *FS = dyn_cast<ForStmt>(FullStmt))
        FullStmt = FS->getBody();
      else if (const WhileStmt *WS = dyn_cast<WhileStmt>(FullStmt))
        FullStmt = WS->getBody();
      else if (const DoStmt *DS = dyn_cast<DoStmt>(FullStmt))
        FullStmt = DS->getBody();
    }

    auto It = std::find_if(
        UpdateToList.begin(), UpdateToList.end(),
        [FullStmt](UpdateDirInfo &U) { return U.FullStmt == FullStmt; });
    if (It == UpdateToList.end()) {
      UpdateToList.emplace_back(UpdateDirInfo(FullStmt));
      It = --(UpdateToList.end());
    }
    It->Decls.insert(Access.VD);
  }

  for (const UpdateDirInfo &Update : UpdateToList) {
    SourceLocation InsertLoc;
    std::string UpdateToDirective;
    std::string ParentIndent =
        getIndentation(SM, Update.FullStmt->getBeginLoc());
    if (const CompoundStmt *Body = dyn_cast<CompoundStmt>(Update.FullStmt)) {
      // Inserting at the top of a loop body.
      InsertLoc = Body->getBeginLoc().getLocWithOffset(1);

      UpdateToDirective = "\n";
      std::string BodyIndent = getBodyIndentation(SM, Body);
      UpdateToDirective += BodyIndent + IndentStep;
      UpdateToDirective += "#pragma omp target update to(";
      for (const ValueDecl *VD : Update.Decls) {
        UpdateToDirective += VD->getNameAsString() + ",";
      }
      UpdateToDirective.back() = ')';

      // Insert a trailing newline if there is text following and on the same
      // line as the opening bracket.
      const char *Source = SM.getCharacterData(InsertLoc);
      SourceLocation EndLoc = Body->getEndLoc().getLocWithOffset(1);
      const char *End = SM.getCharacterData(EndLoc);
      unsigned int TrailingWhitespace = 0;
      while (Source != End && *Source != '\n') {
        if (!isspace(*Source)) {
          UpdateToDirective += "\n";
          UpdateToDirective += BodyIndent + IndentStep;
          R.RemoveText(InsertLoc, TrailingWhitespace);
          break;
        }
        ++TrailingWhitespace;
        ++Source;
      }
    } else {
      // Inserting after a semi-terminated statement.
      InsertLoc = getSemiTerminatedStmtEndLoc(SM, Update.FullStmt);
#if DEBUG_LEVEL >= 1
      llvm::outs() << "Update.FullStmt...";
      Update.FullStmt->printPretty(llvm::outs(), nullptr,
                                   PrintingPolicy(LangOptions()));
      llvm::outs() << "end\n";
      llvm::outs() << "InsertLoc..."
                   << InsertLoc.printToString(Context.getSourceManager())
                   << "\n";
#endif

      UpdateToDirective = "\n";
      UpdateToDirective += ParentIndent + IndentStep;
      UpdateToDirective += "#pragma omp target update to(";
      for (const ValueDecl *VD : Update.Decls) {
        UpdateToDirective += VD->getNameAsString() + ",";
      }
      UpdateToDirective.back() = ')';
      // Insert a trailing newline if there is text following and on the same
      // line as the stmt.
      const char *Source = SM.getCharacterData(InsertLoc);
      FileID FID = SM.getFileID(Update.FullStmt->getEndLoc());
      SourceLocation FileEndLoc = SM.getLocForEndOfFile(FID);
      const char *FileEnd = SM.getCharacterData(FileEndLoc);
      unsigned int TrailingWhitespace = 0;
      while (Source != FileEnd + sizeof(char) && *Source != '\n') {
        if (!isspace(*Source)) {
          UpdateToDirective += "\n";
          UpdateToDirective += ParentIndent + IndentStep;
          R.RemoveText(InsertLoc, TrailingWhitespace);
          break;
        }
        ++TrailingWhitespace;
        ++Source;
      }
    }

    R.InsertTextBefore(InsertLoc, UpdateToDirective);
  }

  return;
}

void rewriteUpdateFrom(Rewriter &R, ASTContext &Context,
                       const TargetDataRegion *Data,
                       const std::string &IndentStep) {
  if (Data->getUpdateFrom().empty())
    return;

  SourceManager &SM = R.getSourceMgr();

  std::vector<UpdateDirInfo> UpdateFromList;
  for (const AccessInfo &Access : Data->getUpdateFrom()) {
    const Stmt *FullStmt = getSemiTerminatedStmt(Context, Access.S);

    if (Access.Barrier == ScopeBarrier::LoopEnd) {
      if (const ForStmt *FS = dyn_cast<ForStmt>(FullStmt))
        FullStmt = FS->getBody();
      else if (const WhileStmt *WS = dyn_cast<WhileStmt>(FullStmt))
        FullStmt = WS->getBody();
    }
    // update from in do loop conditional should always be inserted directly
    // before the end of the loop.
    if (const DoStmt *DS = dyn_cast<DoStmt>(FullStmt))
      FullStmt = DS->getBody();

    auto It = std::find_if(
        UpdateFromList.begin(), UpdateFromList.end(),
        [FullStmt](UpdateDirInfo &U) { return U.FullStmt == FullStmt; });
    if (It == UpdateFromList.end()) {
      UpdateFromList.emplace_back(UpdateDirInfo(FullStmt));
      It = --(UpdateFromList.end());
    }
    It->Decls.insert(Access.VD);
  }

  for (const UpdateDirInfo &Update : UpdateFromList) {
    SourceLocation InsertLoc;
    std::string UpdateFromDirective;
    std::string ParentIndent =
        getIndentation(SM, Update.FullStmt->getBeginLoc());
    if (const CompoundStmt *Body = dyn_cast<CompoundStmt>(Update.FullStmt)) {
      // Inserting at the end of a loop body.

      std::string BodyIndent = getBodyIndentation(SM, Body);
      // Insert a leading newline if there is text preceeding and on the same
      // line as the closing bracket.
      SourceLocation BeginLoc = Body->getBeginLoc();
      const char *Begin = SM.getCharacterData(BeginLoc);
      SourceLocation EndLoc = Body->getEndLoc().getLocWithOffset(-1);
      const char *Source = SM.getCharacterData(EndLoc);
      unsigned int LeadingWhitespace = 0;
      while (Source != Begin - sizeof(char) && *Source != '\n') {
        if (!isspace(*Source)) {
          UpdateFromDirective += "\n";
          UpdateFromDirective += ParentIndent;
          R.RemoveText(EndLoc.getLocWithOffset(0 - LeadingWhitespace),
                       LeadingWhitespace);
          break;
        }
        ++LeadingWhitespace;
        --Source;
      }

      UpdateFromDirective += IndentStep;
      UpdateFromDirective += "#pragma omp target update from(";
      for (const ValueDecl *VD : Update.Decls) {
        UpdateFromDirective += VD->getNameAsString() + ",";
      }
      UpdateFromDirective.back() = ')';
      UpdateFromDirective += "\n";
      if (Body)
        UpdateFromDirective += ParentIndent;
      else
        UpdateFromDirective += BodyIndent;
      UpdateFromDirective += IndentStep;

      InsertLoc = Body->getEndLoc();
    } else {
      // Inserting before a semi-terminated statement.

      // Insert a leading newline if there is text preceeding and on the same
      // line as the stmt.
      FileID FID = SM.getFileID(Update.FullStmt->getBeginLoc());
      SourceLocation FileBeginLoc = SM.getLocForStartOfFile(FID);
      const char *FileBegin = SM.getCharacterData(FileBeginLoc);
      SourceLocation EndLoc =
          Update.FullStmt->getBeginLoc().getLocWithOffset(-1);
      const char *Source = SM.getCharacterData(EndLoc);
      unsigned int LeadingWhitespace = 0;
      while (Source != FileBegin - sizeof(char) && *Source != '\n') {
        if (!isspace(*Source)) {
          UpdateFromDirective += "\n";
          UpdateFromDirective += ParentIndent + IndentStep;
          R.RemoveText(EndLoc.getLocWithOffset(1 - LeadingWhitespace),
                       LeadingWhitespace);
          break;
        }
        ++LeadingWhitespace;
        --Source;
      }

      UpdateFromDirective += "#pragma omp target update from(";
      for (const ValueDecl *VD : Update.Decls) {
        UpdateFromDirective += VD->getNameAsString() + ",";
      }
      UpdateFromDirective.back() = ')';
      UpdateFromDirective += "\n";
      UpdateFromDirective += ParentIndent + IndentStep;

      InsertLoc = Update.FullStmt->getBeginLoc();
    }

    R.InsertTextBefore(InsertLoc, UpdateFromDirective);
  }

  return;
}

void rewriteClauses(Rewriter &R, ASTContext &Context,
                    const TargetDataRegion *Data) {
  if (Data->getFirstPrivate().empty())
    return;

  // Consolidate new clauses so we have a list for each directive.
  std::vector<ClauseDirInfo> DirectiveList;
  for (const ClauseInfo &Clause : Data->getFirstPrivate()) {
    const OMPExecutableDirective *Directive = Clause.Directive;
    auto It = std::find_if(
        DirectiveList.begin(), DirectiveList.end(),
        [Directive](ClauseDirInfo &C) { return C.Directive == Directive; });
    if (It == DirectiveList.end()) {
      DirectiveList.emplace_back(ClauseDirInfo(Directive));
      It = --(DirectiveList.end());
    }
    It->FirstPrivateDecls.insert(Clause.VD);
  }

  for (ClauseDirInfo &Directive : DirectiveList) {
    std::string Clauses;
    if (!Directive.FirstPrivateDecls.empty()) {
      Clauses += " firstprivate(";
      for (const ValueDecl *VD : Directive.FirstPrivateDecls) {
        Clauses += VD->getNameAsString() + ",";
      }
      Clauses.back() = ')';
    }
    R.InsertTextBefore(Directive.Directive->getEndLoc(), Clauses);
  }

  return;
}

void rewriteTargetDataRegion(Rewriter &R, ASTContext &Context,
                             const TargetDataRegion *Data) {
  rewriteClauses(R, Context, Data);

  if (Data->getMapAlloc().empty() && Data->getMapTo().empty() &&
      Data->getMapFrom().empty() && Data->getMapToFrom().empty())
    return;

  SourceManager &SM = R.getSourceMgr();
  const FunctionDecl *FD = Data->getContainingFunction();
  std::string IndentStep = getIndentationStep(SM, FD);

  rewriteDataMap(R, Context, Data, IndentStep);

  rewriteUpdateTo(R, Context, Data, IndentStep);
  rewriteUpdateFrom(R, Context, Data, IndentStep);

  return;
}

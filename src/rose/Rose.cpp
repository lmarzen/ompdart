#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include "RoseASTConsumer.h"

class RoseASTAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<RoseASTConsumer>(&CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
  void PrintHelp(llvm::raw_ostream& ros) {
    ros << "TODO:: Help goes here\n";
    return;
  }

}; // end class RoseASTAction

static FrontendPluginRegistry::Add<RoseASTAction>
X("-rose", "target data analysis");

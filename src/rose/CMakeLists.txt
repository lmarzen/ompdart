cmake_minimum_required(VERSION 3.20)

add_library(rose MODULE
    Rose.cpp
    CommonUtils.cpp
    DataTracker.cpp
    DirectiveRewriter.cpp
    RoseASTConsumer.cpp
    RoseASTVisitor.cpp
    TargetDataScope.cpp
    TargetRegion.cpp
)

if(LLVM_ENABLE_PLUGINS AND (WIN32 OR CYGWIN))
    target_link_libraries(rose PRIVATE
        clangAST
        clangBasic
        clangFrontend
        LLVMSupport
    )
endif()

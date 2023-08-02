cmake_minimum_required(VERSION 3.20)

add_library(rose MODULE
    Rose.cpp
    DataTracker.cpp
    TargetRegion.cpp
    TargetDataScope.cpp
    CommonUtils.cpp
    DirectiveRewriter.cpp
)

if(LLVM_ENABLE_PLUGINS AND (WIN32 OR CYGWIN))
    target_link_libraries(rose PRIVATE
        clangAST
        clangBasic
        clangFrontend
        LLVMSupport
    )
endif()

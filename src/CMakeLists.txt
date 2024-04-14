cmake_minimum_required(VERSION 3.20)

add_library(ompdart MODULE
    OmpDart.cpp
    AnalysisUtils.cpp
    CommonUtils.cpp
    DataTracker.cpp
    DirectiveRewriter.cpp
    Kernel.cpp
    OmpDartASTConsumer.cpp
    OmpDartASTVisitor.cpp
    TargetDataRegion.cpp
)

cmake_minimum_required(VERSION 3.20)

add_library(rose MODULE
    Rose.cpp
    CommonUtils.cpp
    DataTracker.cpp
    DirectiveRewriter.cpp
    Kernel.cpp
    RoseASTConsumer.cpp
    RoseASTVisitor.cpp
    TargetDataRegion.cpp
)

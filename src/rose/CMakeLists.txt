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

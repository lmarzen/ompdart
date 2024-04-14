#ifndef ACCESSINFO_H
#define ACCESSINFO_H

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"

using namespace clang;

constexpr uint8_t A_NOP     = 0b00000000; // No Read/Write Operation
constexpr uint8_t A_RDONLY  = 0b00000001; // Read Operation
constexpr uint8_t A_WRONLY  = 0b00000010; // Write Operation
constexpr uint8_t A_RDWR    = 0b00000011; // ReadWrite Operation
constexpr uint8_t A_UNKNOWN = 0b00000100; // Unknown Operation
constexpr uint8_t A_OFFLD   = 0b00001000; // Offloaded Operation

enum ScopeBarrier : uint8_t {
  None,
  KernelBegin,
  KernelEnd,
  LoopBegin,
  LoopEnd,
  CondBegin,
  CondCase,
  CondFallback,
  CondEnd
};

struct ArrayAccess {
  uint8_t Flags;             // Read/Write operations
  size_t LitLower;           // Array access literal lower bound
  size_t LitUpper;           // Array access literal upper bound
  const ValueDecl *VarLower; // Array access variable lower bound
  const ValueDecl *VarUpper; // Array access variable upper bound
};

struct LoopAccess {
  size_t LitLower;            // Loop literal lower bound
  size_t LitUpper;            // Loop literal upper bound
  const Expr *ExprLower;      // Loop variable lower bound
  const Expr *ExprUpper;      // Loop variable upper bound
  const ValueDecl *IndexDecl; // Loop index variable
  int8_t LowerOffByOne;       // Compensation for off by one comparisons.
  int8_t UpperOffByOne;       // Compensation for off by one comparisons.
};

struct AccessInfo {
  const ValueDecl *VD;
  const Stmt *S;
  SourceLocation Loc;
  uint8_t Flags;        // Read/Write operations
  ScopeBarrier Barrier; // Indicates begin/end of a block scope
  const ArraySubscriptExpr *ArraySubscript;
  std::vector<ArrayAccess> ArrayBounds;
  const LoopAccess *LoopBounds;
};

#endif
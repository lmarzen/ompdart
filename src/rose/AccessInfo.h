#ifndef ACCESSINFO_H
#define ACCESSINFO_H

#include "clang/AST/Decl.h"

using namespace clang;

constexpr uint8_t A_NOP     =  0b00000000; // No Read/Write Operation
constexpr uint8_t A_RDONLY  =  0b00000001; // Read Operation
constexpr uint8_t A_WRONLY  =  0b00000010; // Write Operation
constexpr uint8_t A_RDWR    =  0b00000011; // ReadWrite Operation
constexpr uint8_t A_UNKNOWN =  0b00000100; // Unknown Operation
constexpr uint8_t A_OFFLD   =  0b00001000; // Offloaded Operation
constexpr uint8_t A_LBEGIN  =  0b00100000; // Loop Begin Marker
constexpr uint8_t A_LEND    =  0b00010000; // Loop End Marker

struct AccessInfo {
  ValueDecl *VD;
  Stmt *S;
  SourceLocation Loc;
  uint8_t Flags;

  // comparison operators - compare by source location
  friend bool operator<(const AccessInfo &L, const AccessInfo &R) {
    return L.Loc.getRawEncoding() < R.Loc.getRawEncoding();
  }
  friend bool operator>(const AccessInfo &L, const AccessInfo &R) {
    return L.Loc.getRawEncoding() > R.Loc.getRawEncoding();
  }
  friend bool operator<=(const AccessInfo &L, const AccessInfo &R) {
    return L.Loc.getRawEncoding() <= R.Loc.getRawEncoding();
  }
  friend bool operator>=(const AccessInfo &L, const AccessInfo &R) {
    return L.Loc.getRawEncoding() >= R.Loc.getRawEncoding();
  }
  friend bool operator==(const AccessInfo &L, const AccessInfo &R) {
    return L.Loc.getRawEncoding() == R.Loc.getRawEncoding();
  }
  friend bool operator!=(const AccessInfo &L, const AccessInfo &R) {
    return L.Loc.getRawEncoding() != R.Loc.getRawEncoding();
  }
};

#endif
#pragma once
#include "../include/message.h"
// #include <sstream>

const char* to_string(BFTPhase t) {
  switch (t) {
    case BFTPhase::PrePrepare: return "PRE_PREPARE";
    case BFTPhase::Prepare:    return "PREPARE";
    case BFTPhase::Commit:     return "COMMIT";
    case BFTPhase::Shutdown:   return "SHUTDOWN";
    default:                      return "UNKNOWN";
  }
}



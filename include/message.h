#pragma once
#include <cstdint>
#include <iostream>
#include <string>
#include <sstream>


//The blockdata that we will agree on
struct BlockData {
  uint32_t value;
  //Can add a hash later
};

inline bool operator==(const BlockData& a, const BlockData& b) { return a.value == b.value; }
inline bool operator<(const BlockData& a, const BlockData& b) { return a.value < b.value; }

enum class BFTPhase : uint32_t {
  PrePrepare = 0,
  Prepare    = 1,
  Commit     = 2,
  Shutdown   = 3,
};

struct BFTProposal {
  BFTPhase phase;
  uint32_t view;
  uint32_t slot;
  BlockData value; //Could be replaced by hash
};

struct ProposalId {
  uint32_t view;
  uint32_t slot;
  BlockData value; //We will replace this with a hash of blockdata
};

inline ProposalId proposal_key(const BFTProposal& b){ return ProposalId {b.view, b.slot, b.value};}
inline bool operator==(const ProposalId& a, const ProposalId& b){
  return (a.slot == b.slot) && (a.value == b.value) && (a.view == b.view);
}

inline bool operator<(const ProposalId& a, const ProposalId& b) {
  if (a.view != b.view) return a.view < b.view;
  if (a.slot!= b.slot) return a.slot< b.slot;
  return a.value < b.value;
}


// Strict weak ordering for map keys
inline bool operator<(const BFTProposal& a, const BFTProposal& b) {
  if (a.view        != b.view)        return a.view        < b.view;
  if (a.slot != b.slot) return a.slot < b.slot;
  if (a.phase        != b.phase)        return static_cast<uint32_t>(a.phase) < static_cast<uint32_t>(b.phase);
  return a.value < b.value;
}

inline bool operator==(const BFTProposal& a, const BFTProposal& b) {
  return a.view == b.view &&
         a.slot == b.slot &&
         a.phase == b.phase &&
         a.value == b.value;
}
inline bool operator!=(const BFTProposal& a, const BFTProposal& b) { return !(a == b); }

struct P2PMessage {
  uint32_t from;
  uint32_t to;   // for broadcast you can ignore `to` or set to UINT32_MAX
  BFTProposal proposal;
};


const char* to_string(BFTPhase t);

inline std::ostream& operator<<(std::ostream& os, BFTPhase t) {
  return os << to_string(t);
}

inline std::ostream& operator<<(std::ostream& os, const BlockData& v) {
  return os << v.value;
}

inline std::ostream& operator<<(std::ostream& os, const BFTProposal& b) {
  return os << "Block{type=" << to_string(b.phase)
            << ", view=" << b.view
            << ", instance_id=" << b.slot
            << ", value=" << b.value
            << "}";
}

inline std::ostream& operator<<(std::ostream& os, const P2PMessage& m) {
  return os << "Message{from=" << m.from
            << ", to=" << m.to
            << ", block=" << m.proposal
            << "}";
}

inline std::string block_string(const BFTProposal& b) {
  std::ostringstream oss;
  oss << "view=" << b.view
      << " instance=" << b.slot
      << " type=" << to_string(b.phase)
      << " value=" << b.value.value;
  return oss.str();
}



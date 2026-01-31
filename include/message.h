#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

typedef std::array<uint8_t, 32> BlockHash;

const BlockHash GENESIS_HASH = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// The blockdata that we will agree on
struct Block {
  uint32_t height{0};
  BlockHash previous_hash = GENESIS_HASH;
  std::string transactions;

  BlockHash hash_block_data_to_bytes();
  std::vector<uint8_t> to_payload() const;
  static std::optional<Block> from_payload(std::vector<uint8_t>);
};

inline bool operator==(const Block &a, const Block &b) {
  return a.transactions == b.transactions && a.height == b.height &&
         a.previous_hash == b.previous_hash;
}

inline bool operator<(const Block &a, const Block &b) {
  return a.transactions < b.transactions;
}

// Eventually this could be squeezed in a less than a byte
enum class BFTPhase : uint8_t {
  // PrePrepare = 0, //This will be replaced by the Block Proposal struct
  Prepare = 0,
  Commit = 1,
};

struct BFTVote {
  uint32_t leader;
  uint32_t slot;
  std::vector<uint32_t> votes;
  BFTPhase phase;
  BlockHash blockhash; // Could be replaced by hash

  std::vector<uint8_t> to_payload() const;
  static std::optional<BFTVote> from_payload(const std::vector<uint8_t> &);
};

struct BlockProposal {
  uint32_t view;
  uint32_t slot;
  Block block;

  std::vector<uint8_t> to_payload() const;
  static std::optional<BlockProposal>
  from_payload(const std::vector<uint8_t> &);
};

// inline BlockProposal proposal_key(const BFTVote &b) {
//   return BlockProposal{b.view, b.slot, b.block};
// }

inline bool operator==(const BlockProposal &a, const BlockProposal &b) {
  return (a.slot == b.slot) && (a.block == b.block) && (a.view == b.view);
}

inline bool operator<(const BlockProposal &a, const BlockProposal &b) {
  if (a.view != b.view)
    return a.view < b.view;
  if (a.slot != b.slot)
    return a.slot < b.slot;
  return a.block < b.block;
}

// Strict weak ordering for map keys
inline bool operator<(const BFTVote &a, const BFTVote &b) {
  if (a.leader != b.leader)
    return a.leader < b.leader;
  if (a.slot != b.slot)
    return a.slot < b.slot;
  if (a.phase != b.phase)
    return static_cast<uint32_t>(a.phase) < static_cast<uint32_t>(b.phase);
  return a.blockhash < b.blockhash;
}

inline bool operator==(const BFTVote &a, const BFTVote &b) {
  return a.leader == b.leader && a.slot == b.slot && a.phase == b.phase &&
         a.blockhash == b.blockhash;
}

inline bool operator!=(const BFTVote &a, const BFTVote &b) { return !(a == b); }

enum class MessageKind {
  BlockProposalMessage, // Proposes a full block
  BFTVoteMessage,       // Just a vote
  TxGossipMessage,      // Tx propagation (int the future)
};

struct P2PMessage {
  uint32_t from;
  uint32_t to; //
  MessageKind msg_kind;
  std::vector<uint8_t> payload; // Payload for universal communication
};

// For serialzing payloads
class Writer {
public:
  Writer() = default;
  std::vector<uint8_t> to_payload();

  std::vector<uint8_t> buffer;

  void u32(uint32_t n) {
    buffer.push_back(uint8_t(n & 0xFF));
    buffer.push_back(uint8_t(n >> 8) & 0xFF);
    buffer.push_back(uint8_t(n >> 16) & 0xFF);
    buffer.push_back(uint8_t(n >> 24) & 0xFF);
  }
  void write_u32_vector(const std::vector<uint32_t> &vec) {
    u32(static_cast<uint32_t>(vec.size()));
    for (uint32_t k : vec) {
      u32(k);
    }
  }

  void write_bytes(const uint8_t *p, size_t n) {
    buffer.insert(buffer.end(), p, p + n);
  };

  void write_hash(const BlockHash &hash) {
    buffer.insert(buffer.end(), hash.data(), hash.data() + hash.size());
  }

  void write_str(const std::string &s) {
    u32(s.size());
    write_bytes(reinterpret_cast<const uint8_t *>(s.data()), s.size());
  }

  void write_payload(const std::vector<uint8_t> &payload) {
    buffer.insert(buffer.end(), payload.begin(), payload.end());
  }
  void write_phase(const BFTPhase phase) {
    uint8_t phase_val;
    if (phase == BFTPhase::Prepare) {
    phase_val = 0;
    } else {
      phase_val = 1;
    }
    buffer.push_back(phase_val);
  }
};

class Reader {
public:
  Reader(const std::vector<uint8_t> &v) : p(v.data()), n(v.size()){};

  const uint8_t *p{};
  size_t n{};
  bool require(size_t k) {
    return k <= n;
  } // Method to check that we have a correct length
  std::optional<uint32_t> read_u32() {
    if (!require(4))
      return std::nullopt;
    uint32_t v = uint32_t(p[0]) | uint32_t(p[1] << 8) | uint32_t(p[2] << 16) |
                 uint32_t(p[3] << 24);
    p += 4;
    n -= 4;
    return v;
  }

  std::optional<std::vector<uint32_t>> read_u32_vector() {
    auto size = read_u32();
    if (!size)
      return std::nullopt;
    std::vector<uint32_t> vec;
    vec.resize(*size);
    for (uint32_t i = 0; i < *size; ++i) {
      vec[i] = *read_u32();
    }
    return vec;
  }
  std::optional<BlockHash> read_blockhash() {
    if (!require(32))
      return std::nullopt;
    BlockHash b{};
    memcpy(b.data(), p, 32);
    p += 32;
    n -= 32;
    return b;
  }
  std::optional<std::string> read_string() {
    auto lenOpt = read_u32();
    if (!lenOpt)
      return std::nullopt;
    uint32_t len = *lenOpt;
    if (!require(len))
      return std::nullopt;
    std::string s(reinterpret_cast<const char *>(p),
                  reinterpret_cast<const char *>(p + len));
    p += len;
    n -= len;
    return s;
  }

  std::optional<BFTPhase> read_phase() {
    BFTPhase vote;

    if (p[0] == 0) {
      vote = BFTPhase::Prepare;
    } else {
      vote = BFTPhase::Commit;
    }
    p += 1;
    n -= 1;
    return std::optional<BFTPhase>(vote);
  }
};

const char *to_string(BFTPhase t);


std::string message_kind_to_string(MessageKind);

inline std::ostream &operator<<(std::ostream &os, const MessageKind &m) {
  return os << "MessageKind=" << message_kind_to_string(m);
  {}
}

inline std::ostream &operator<<(std::ostream &os, const P2PMessage &m) {
  return os << "Message{from=" << m.from << ", to=" << m.to
            << ", block=" << m.msg_kind << "}";
}

std::string to_hex(const uint8_t *data, size_t n);

inline std::ostream &operator<<(std::ostream &os, const BlockHash &bh) {
  return os << to_hex(bh.data(), bh.size());
}

// Print Block fully
inline std::ostream &operator<<(std::ostream &os, const Block &b) {
  os << "Block{height=" << b.height << ", previous_hash=" << b.previous_hash
     << ", transactions=\"" << b.transactions << "\"}";
  return os;
}

// Helper: print vector<uint32_t> nicely
inline std::ostream &operator<<(std::ostream &os,
                                const std::vector<uint32_t> &v) {
  os << "[";
  for (size_t i = 0; i < v.size(); ++i) {
    os << v[i];
    if (i + 1 < v.size())
      os << ", ";
  }
  os << "]";
  return os;
}

// Print BFTVote fully
inline std::ostream &operator<<(std::ostream &os, const BFTVote &v) {
  os << "BFTVote{leader=" << v.leader << ", slot=" << v.slot
     << ", phase=" << to_string(v.phase) << ", blockhash=" << v.blockhash
     << ", votes=" << v.votes << "}";
  return os;
}

// Print BlockProposal fully
inline std::ostream &operator<<(std::ostream &os, const BlockProposal &p) {
  os << "BlockProposal{view=" << p.view << ", slot=" << p.slot
     << ", block=" << p.block << "}";
  return os;
}

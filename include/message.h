#pragma once
#include <cstdint>
#include <iostream>
#include <string>

struct Value {
  uint32_t value;
};

inline bool operator==(const Value& a, const Value& b) { return a.value == b.value; }
inline bool operator<(const Value& a, const Value& b) { return a.value < b.value; }

enum class MessageType : uint32_t {
  PrePrepare = 0,
  Prepare    = 1,
  Commit     = 2,
  Shutdown   = 3,
};

struct Block {
  MessageType type;
  uint32_t view;
  uint32_t instance_id;
  Value value;
};

// Strict weak ordering for map keys
inline bool operator<(const Block& a, const Block& b) {
  if (a.view        != b.view)        return a.view        < b.view;
  if (a.instance_id != b.instance_id) return a.instance_id < b.instance_id;
  if (a.type        != b.type)        return static_cast<uint32_t>(a.type) < static_cast<uint32_t>(b.type);
  return a.value < b.value;
}

inline bool operator==(const Block& a, const Block& b) {
  return a.view == b.view &&
         a.instance_id == b.instance_id &&
         a.type == b.type &&
         a.value == b.value;
}
inline bool operator!=(const Block& a, const Block& b) { return !(a == b); }

struct Message {
  uint32_t from;
  uint32_t to;   // for broadcast you can ignore `to` or set to UINT32_MAX
  Block block;
};

const char* to_string(MessageType t);

std::ostream& operator<<(std::ostream&, MessageType);
std::ostream& operator<<(std::ostream&, const Value&);
std::ostream& operator<<(std::ostream&, const Block&);
std::ostream& operator<<(std::ostream&, const Message&);

std::string block_string(const Block& b);


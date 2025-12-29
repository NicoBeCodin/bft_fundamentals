#include "../include/message.h"
#include <sstream>

const char* to_string(MessageType t) {
  switch (t) {
    case MessageType::PrePrepare: return "PRE_PREPARE";
    case MessageType::Prepare:    return "PREPARE";
    case MessageType::Commit:     return "COMMIT";
    case MessageType::Shutdown:   return "SHUTDOWN";
    default:                      return "UNKNOWN";
  }
}

std::ostream& operator<<(std::ostream& os, MessageType t) {
  return os << to_string(t);
}

std::ostream& operator<<(std::ostream& os, const Value& v) {
  return os << v.value;
}

std::ostream& operator<<(std::ostream& os, const Block& b) {
  return os << "Block{type=" << to_string(b.type)
            << ", view=" << b.view
            << ", instance_id=" << b.instance_id
            << ", value=" << b.value
            << "}";
}

std::ostream& operator<<(std::ostream& os, const Message& m) {
  return os << "Message{from=" << m.from
            << ", to=" << m.to
            << ", block=" << m.block
            << "}";
}

std::string block_string(const Block& b) {
  std::ostringstream oss;
  oss << "view=" << b.view
      << " instance=" << b.instance_id
      << " type=" << to_string(b.type)
      << " value=" << b.value.value;
  return oss.str();
}


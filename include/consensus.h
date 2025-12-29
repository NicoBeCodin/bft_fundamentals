#pragma once
//Plain virtual interface for the consensus algorithm that we will assign to the nodes
#include <cstdint>
#include <optional>

class Node;
class Message;
class Value;

class IConsensus {
public:
  virtual ~IConsensus()=default;
  virtual void on_start(Node& node) = 0;

  //0 for success
  // 1 for not yet treatable
  // 2 for invalid
  virtual uint8_t handle_message(const Message& msg, Node& node) = 0;
  
};


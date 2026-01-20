#pragma once
//Plain virtual interface for the consensus algorithm that we will assign to the nodes
#include <cstdint>
#include <optional>

class Node;
class P2PMessage;
class BlockData;

class IConsensus {
public:
  virtual ~IConsensus()=default;
  virtual void on_start(Node& node) = 0;

  //0 for success
  // 1 for not yet treatable
  // 2 for invalid
  virtual uint8_t handle_message(const P2PMessage& msg, Node& node) = 0; //A node will wait on this function
  
};


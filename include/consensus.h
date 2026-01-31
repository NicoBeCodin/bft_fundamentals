#pragma once
//Plain virtual interface for the consensus algorithm that we will assign to the node
#include "message.h"
#include <cstdint>
#include <optional>

class Node;
class P2PMessage;
class Block;

class IConsensus {
public:
  virtual ~IConsensus()=default;
  virtual void on_start(Node& node) = 0;
  virtual Block last_commited_block() = 0;
  virtual uint32_t get_leader() = 0;
  virtual uint32_t get_slot() = 0;
  virtual BlockProposal create_random_block(uint32_t, uint32_t) = 0;


  //0 for success
  // 1 for not yet treatable
  // 2 for invalid
  virtual uint8_t handle_message(const P2PMessage& msg, Node& node) = 0; //A node will wait on this function
  
};


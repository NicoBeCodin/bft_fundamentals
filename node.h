
#include <atomic>
#include <map>
#include <set>
#include <thread>
#include <iostream>
#include <syncstream>

#include "network.h"
class Node {
public:
  
  Node(uint32_t id, Network &network)
      : m_id(id), m_network(network), m_running(true),
        m_thread(&Node::run, this) {}

  ~Node() { stop(); }
  void stop();
  void send_to(uint32_t id, Block payload);
  void broadcast(Block payload);
  Block pre_prepare_block(Value value);
  Block prepare_block(Value value);
  Block commit_block(Value value);
  void treat_message(Message msg);
  void print_message(const Message &msg);
  void print_string(const std::string string);
  uint32_t id() const noexcept;
  

private:
  void run();
  uint32_t m_id;
  Network &m_network;
  std::atomic<bool> m_running;
  std::thread m_thread;
  uint32_t view = 0;
  uint32_t instance_id = 0;
  //We store the recent values for each
  std::unique_ptr<Block> pre_prepared_recent;
  std::unique_ptr<Block> prepared_recent;
  std::unique_ptr<Block> commit_recent;

  //To record votes for a block
  std::map<Block, std::set<uint32_t>> prepare_votes;
  std::map<Block, std::set<uint32_t>> commit_votes;
  
  //We map a instance id to a block
  std::map<uint32_t, Block> commited_blocks;

    

};

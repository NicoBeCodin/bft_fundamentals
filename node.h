
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
      : m_id(id), m_network(network), m_running(true) {}
  void start();
  ~Node() { stop(); }
  void stop();
  void send_to(uint32_t id, Block payload);
  void propose_block(Block block);
  void broadcast(Block block);
  Block pre_prepare_block(Value value);
  Block prepare_block(Value value);
  Block commit_block(Value value);
  uint8_t treat_message(Message msg);
  void treat_message_queue();
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
  //
  std::deque<Message> untreated;
  std::map<uint32_t, std::vector<Message>> msg_for_instance;
  
  std::unique_ptr<Block> pre_prepared_recent;

  //To record votes for a block
  std::map<Block, std::set<uint32_t>> prepare_votes;
  std::map<Block, std::set<uint32_t>> commit_votes;
  
  //We map a instance id to a block
  std::map<uint32_t, Block> commited_blocks;

  //For out of order messages we shall receive
  std::map<Block, std::vector<Message>> pending_prepare_messages;
  std::map<Block, std::vector<Message>> pending_commit_messages;

  
};

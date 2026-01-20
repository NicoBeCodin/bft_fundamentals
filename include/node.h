#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <syncstream>

#include "consensus.h"
#include "message.h"
#include "transport.h"

class Node {
public:
  Node(uint32_t id,
       size_t cluster_size,
       std::unique_ptr<ITransport> transport,
       std::unique_ptr<IConsensus> consensus);

  ~Node();

  void start();
  void stop();

  uint32_t id() const noexcept { return id_; }
  uint32_t get_quorum_size() const;

  void broadcast(const BFTProposal& block);
  size_t get_cluster_size(){ return transport_->cluster_size();}

  // Called by transport thread(s) when a message arrives
  void on_receive(P2PMessage&& msg);

  // Consensus uses these
  // void send_to(uint32_t to, Block block);
  // void broadcast(Block block);

  void print_message(const P2PMessage& msg);
  void print_string(const std::string& s);

  void run();
  void treat_message_queue();

private:
  uint32_t id_;
  size_t cluster_size_;

  std::unique_ptr<ITransport> transport_;
  std::unique_ptr<IConsensus> consensus_;
  // bool debug = true;

  std::thread worker_;
  std::atomic<bool> running_{false};

  std::mutex q_mtx_;
  std::condition_variable q_cv_;
  std::deque<P2PMessage> untreated_;
};

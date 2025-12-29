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

  // Called by transport thread(s) when a message arrives
  void on_receive(Message&& msg);

  // Consensus uses these
  void send_to(uint32_t to, Block block);
  void broadcast(Block block);

  void print_message(const Message& msg);
  void print_string(const std::string& s);

private:
  void run();
  void treat_message_queue();

private:
  uint32_t id_;
  size_t cluster_size_;

  std::unique_ptr<ITransport> transport_;
  std::unique_ptr<IConsensus> consensus_;

  std::atomic<bool> running_{false};
  std::thread worker_;

  std::mutex q_mtx_;
  std::condition_variable q_cv_;
  std::deque<Message> untreated_;
};

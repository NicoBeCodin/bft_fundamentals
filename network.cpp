#include "network.h"
#include <cstring>
#include <stdexcept>
#include <syncstream>


template <typename T>
void ThreadSafeQueue<T>::push(T value) {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push_back(std::move((value)));
  }
  m_cv.notify_one();
};
template <typename T>
T ThreadSafeQueue<T>::wait_and_pop() {
  std::unique_lock<std::mutex> lock(m_mutex);
  m_cv.wait(lock, [this] { return !m_queue.empty(); });
  T value = std::move(m_queue.front());
  m_queue.pop_front();
  return value;
}

Network::Network(std::size_t num_nodes) {
  if (num_nodes < MIN_NODES) {
    std::cerr << "Not enough nodes for BFT, mininmum is " << MIN_NODES <<std::endl;
    throw;
  }
  inboxes.reserve(num_nodes);
  for (uint32_t i = 0; i < num_nodes; ++i) {
    inboxes.push_back(std::make_unique<ThreadSafeQueue<Message>>());
  }
};

void Network::send(const Message &msg) {
  if (static_cast<uint32_t>(inboxes.size()) <= msg.to) {
    throw std::out_of_range("Invalid destination id");
  }
  inboxes[msg.to]->push(msg);
}

Message Network::recv(uint32_t node_id) {
  if (node_id >= inboxes.size()) {
    throw std::out_of_range("Invalid receiver id");
  }
  return inboxes[node_id]->wait_and_pop();
}

void Network::broadcast(uint32_t from, Block payload) {
  for (uint32_t i = 0; i < inboxes.size(); ++i) {
    if (i == from)
      continue;
    send(Message{from, i, payload});
  }
}

std::ostream& operator<<(std::ostream& stream, const Value &val) {
  return stream << val.value;
}

std::ostream& operator<<(std::ostream& stream, const Block &block){
  return stream << "Block{type=" << to_string(block.type) << ", view=" << block.view << ", instance_id=" << block.instance_id << ", value="<< block.value<<"}";
}


std::ostream& operator<<(std::ostream& stream, const Message &msg){
  return stream << "Message{to=" << msg.to << ", from=" << msg.from << ", block=" << msg.block <<"}";  
}


void print_message_from_node(uint32_t node, const Message &msg) {
  std::osyncstream bout(std::cout);
  bout<<"Node: " << node<<' '<< msg << std::endl;
};

void print_string_from_node(uint32_t node, const std::string str){
  std::osyncstream bout(std::cout);
  bout << "Node: " << node << ' ' << str << std::endl;
}


std::string block_string(const Block b) {
  std::string block_string = "Block view: " + std::to_string(b.instance_id) +
                             " Instance: " + std::to_string(b.instance_id) +
                             " Value: " + std::to_string(b.value.value);
  return block_string;
}

size_t Network::size() const noexcept { return inboxes.size(); }

size_t Network::f_size() const noexcept {
  size_t f = size() - 1 / 3;
  return f;
};

size_t Network::quorum_size() const noexcept {
  size_t quorum_size = 2 * f_size() + 1;
  return quorum_size;
}

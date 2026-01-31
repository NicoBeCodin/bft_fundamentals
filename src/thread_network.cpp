#pragma once
#include "../include/thread_network.h"
#include <cstring>
#include <stdexcept>


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

template <typename T>
size_t ThreadSafeQueue<T>::current_size(){
  std::unique_lock<std::mutex> lock(m_mutex);
  return m_queue.size();
}

void ThreadTransport::start(uint32_t node_id){
   // size_t current_size=  shared_inboxes->inboxes[node_id]->current_size();
   node_id = node_id;
   // std::cout << "Node: " << node_id << " initialized inbox, current size: " << current_size << std::endl;
   
}
void ThreadTransport::stop(){
  std::cout << "Stopping " << std::endl;
}


void ThreadTransport::send(const P2PMessage msg) {
  if (static_cast<uint32_t>(shared_inboxes->inboxes.size()) <= msg.to) {
    throw std::out_of_range("Invalid destination id");
  }
  shared_inboxes->inboxes[msg.to]->push(msg);
}


P2PMessage ThreadTransport::recv() {
  if (node_id >= shared_inboxes->inboxes.size()) {
    throw std::out_of_range("Invalid receiver id");
  }
  return shared_inboxes->inboxes[node_id]->wait_and_pop();
}

void ThreadTransport::broadcast(const P2PMessage msg) {
  for (uint32_t i= 0; i < shared_inboxes->inboxes.size(); ++i) {
    if (i == node_id)
      continue;
    P2PMessage new_msg = msg;
    new_msg.to = i;
    send(new_msg);
  }
}

size_t ThreadTransport::cluster_size() const { return shared_inboxes->inboxes.size(); }


#pragma once
#include <iostream>
#include <compare>
#include <vector>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <string>
#include <sstream>
#include "transport.h"

const size_t MIN_NODES = 4;

//Each thread node has its queue that acts receiving messages
template <typename T> class ThreadSafeQueue {
public:
  void push(T value);
  T wait_and_pop();
  size_t current_size(); //Get number of elements in queue

private:
  std::deque<T> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_cv;
};


//this object will be shared across the different threads
struct SharedInboxes {
  explicit SharedInboxes(size_t n_nodes){
    if (n_nodes < MIN_NODES){
      throw std::runtime_error("Not enough nodes!");
    }
    inboxes.reserve(n_nodes);
    for (size_t i = 0; i < n_nodes; ++i){
      inboxes.push_back(std::make_unique<ThreadSafeQueue<P2PMessage>>());
    }
  }
  std::vector<std::unique_ptr<ThreadSafeQueue<P2PMessage>>> inboxes;
};


class ThreadTransport: public ITransport{
  public:
    explicit ThreadTransport(uint32_t node_id, std::shared_ptr<SharedInboxes> inboxes): node_id(node_id), shared_inboxes(inboxes){
    };
    //Dummy functions (not used for thread communications)
    void start(uint32_t node_id) override; //node id of node
    void stop() override;
    
    void send(const P2PMessage msg) override;
    P2PMessage recv() override;
    void broadcast(const P2PMessage msg) override;
    
    size_t size() const noexcept;
    size_t f_size() const noexcept;
    size_t cluster_size() const override;    
    
    
  private:
    std::shared_ptr<SharedInboxes> shared_inboxes;
    uint32_t node_id;
    
};



//The network will pass payloads, the item on which to achieve consensus will be the block


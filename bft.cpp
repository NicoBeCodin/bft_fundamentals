#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <vector>

using namespace std;

template <typename T>
class ThreadSafeQueue{
public:
  void push(T value){
    {
    lock_guard<std::mutex> lock(mutex);
    queue.push_back(std::move((value)));
  }
  cv.notify_one();
  }

  T wait_and_pop(){
    unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] {
              return !queue.empty();
            });
    T value = std::move(queue.front());
    queue.pop_front();
    return value;
  }

private:
  deque<T> queue;
  mutex mutex;
  condition_variable cv;
  
};

struct Message {
  int from;
  int to;
  string payload;
};

class Network{
public:
  explicit Network(size_t num_nodes) {
    inboxes.resize(num_nodes);  
  };

  void send(const Message& msg){
    if (msg.to < 0 || static_cast<int>(inboxes.size()) <= msg.to){
      throw std::out_of_range("Invalid destination id");
    }
    inboxes[msg.to].push(msg);
  }

  Message recv(int node_id){
    if (node_id < 0 || node_id >= inboxes.size()){
      throw std::out_of_range("Invalid receiver id");
    }
    return inboxes[node_id].wait_and_pop();
  }

  void broadcast(int from, string payload){
    for (int i =0; i < inboxes.size(); ++i){
      if (i == from) continue;
      send(Message{from, i, payload});
    }
  }

  size_t size() const noexcept {return inboxes.size();}
//How the nodes send messages to each other
private:
  vector<ThreadSafeQueue<Message>> inboxes;
};


class Node {
  public:
    Node(int id, Network &network) :
      m_id(id), m_network(network), m_running(true), m_thread(&Node::run, this){
        
      }

  ~Node(){
    stop();
  }

  int id()const noexcep
  
};




int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "Usage: ./bft n ";
  }
  int n = atoi(argv[1]);
  cout << "Creating "<< n << " threads "<< endl;
  
  vector<thread> threads;
  threads.reserve(n);
  
  
    

  

  return 0;
}

#include <iostream>
#include <compare>
#include <vector>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <string>

const size_t MIN_NODES = 4;


template <typename T> class ThreadSafeQueue {
public:
  void push(T value);
  T wait_and_pop();

private:
  std::deque<T> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_cv;
};

struct Value {
  uint32_t value;

  // bool operator!=(const Value&) const =default; 
  bool operator==(const Value&) const =default; 
};

enum MessageType {
  PrePrepare,
  Prepare,
  Commit,
  Shutdown, //For shutting down a node
};

//We call this a block
// This is the object we are acheiving consensus on
struct Block {
  MessageType type;
  uint32_t view;
  uint32_t instance_id;
  Value value;

  bool operator==(const Block&) const =default;
};

inline bool operator<(const Block& a, const Block& b) {
  // Order by the same fields you consider for equality (and more if needed)
  return std::tie(a.view, a.instance_id, a.type, a.value.value)
       < std::tie(b.view, b.instance_id, b.type, b.value.value);
}

inline bool operator!=(const Block& a, const Block& b) { return !(a == b); }

inline const char* to_string(MessageType t);

struct Message {
  uint32_t from;
  uint32_t to;
  Block block;
};

inline const char* to_string(MessageType t){
  switch(t){
    case MessageType::PrePrepare: return "PRE_PREPARE";
    case MessageType::Prepare: return "PREPARE";
    case MessageType::Commit: return "COMMIT";
    case MessageType::Shutdown: return "SHUTDOWN";
    default: return "ERROR";
  
  }
};


std::ostream& operator<<(std::ostream&, MessageType);
std::ostream& operator<<(std::ostream&, const Value&);
std::ostream& operator<<(std::ostream&, const Block&);
std::ostream& operator<<(std::ostream&, const Message&);


class Network{
  public:
    explicit Network(std::size_t num_nodes);
    void send(const Message& msg);
    Message recv(uint32_t node_id);
    void broadcast(uint32_t from, Block block);
    std::size_t size() const noexcept;
    std::size_t f_size() const noexcept;
    std::size_t quorum_size() const noexcept;
    
  private:
    std::vector<std::unique_ptr<ThreadSafeQueue<Message>>> inboxes;  
};

//The network will pass payloads, the item on which to achieve consensus will be the block


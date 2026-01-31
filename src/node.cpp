#include "../include/node.h"
#include <iostream>

Node::Node(uint32_t id,
           size_t cluster_size,
           std::unique_ptr<ITransport> transport,
           std::unique_ptr<IConsensus> consensus)
  : id_(id),
    cluster_size_(cluster_size),
    transport_(std::move(transport)),
    consensus_(std::move(consensus)) {
      
    }

Node::~Node() {
  stop();
}

uint32_t Node::get_quorum_size() const {
  // PBFT quorum = 2f+1 where f=(N-1)/3
  const uint32_t N = static_cast<uint32_t>(cluster_size_);
  const uint32_t f = (N > 0) ? (N - 1) / 3 : 0;
  return 2 * f + 1;
}

void Node::start() {
  if (running_.exchange(true)) return;
  transport_->start(id());
  worker_ = std::thread(&Node::run, this);
}

void Node::stop() {
  if (!running_.exchange(false)) return;

  // Wake worker
  q_cv_.notify_all();

  if (worker_.joinable()) worker_.join();
  transport_->stop();
}

void Node::on_receive(P2PMessage&& msg) {
  {
    std::lock_guard<std::mutex> lk(q_mtx_);
    untreated_.push_front(std::move(msg));
  }
  q_cv_.notify_one();
}
void Node::broadcast(BlockProposal bp){
  P2PMessage msg{
    id_,
    0,
    MessageKind::BlockProposalMessage,
    std::move(bp.to_payload()),
};
transport_->broadcast(msg);
}

void Node::broadcast(const BFTVote& vote){
  P2PMessage msg{
    id_,
    0,
    MessageKind::BFTVoteMessage,
    std::move(vote.to_payload()),
  };
  transport_->broadcast(msg);
}


// void Node::print_message(const P2PMessage& m) {
//   std::osyncstream bout(std::cout);
//   bout << "Node: " << id_ << " " << m << "\n";
// }

void Node::print_string(const std::string& s) {
  std::osyncstream bout(std::cout);
  bout << "Node: " << id_ << " " << s << "\n";
}


void Node::propose_random_block(){
  BlockProposal bp = consensus_->create_random_block(consensus_->get_leader(), consensus_->get_slot());
  //Testing
  std::ostringstream oss;
  oss << bp;
  auto before = oss.str();
  auto vec  = bp.to_payload();
  auto after = *BlockProposal::from_payload(vec);
  std::ostringstream osss;
  
  osss << after;
  auto after_string = osss.str();
  print_string(before);
  print_string(after_string);
  
  
    
  
  
  
  broadcast(bp);
}

void Node::run() {
  
  std::string start_string = "Node " + std::to_string(id_) + " started";
  print_string(start_string);
  consensus_->on_start(*this);

  while (running_.load()) {
    P2PMessage msg = transport_->recv();
    on_receive(std::move(msg));  
    // Process queue under lock
    treat_message_queue();
  }

  print_string("Shutting down...");
}

void Node::treat_message_queue() {
  bool progress = true;

  while (!untreated_.empty() && progress) {
    progress = false;
    const std::size_t n = untreated_.size();

    for (std::size_t k = 0; k < n; ++k) {
      P2PMessage msg = std::move(untreated_.front());
      untreated_.pop_front();

      // 0 = handled; 1 = retry later; 2 = invalid drop
      uint8_t rc = consensus_->handle_message(msg, *this);

      if (rc == 0) {
        progress = true;
      } else if (rc == 1) {
        untreated_.push_back(std::move(msg));
      } else {
        print_string("WEIRD CASE");
        // drop invalid
      }
    }
  }
}

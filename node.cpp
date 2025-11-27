#include "node.h"

void Node::stop() {
  bool expected = true;
  if (m_running.compare_exchange_strong(expected, false)) {
    Block payload = Block{
        MessageType::Shutdown,
        0,
        0,
        0,
    };
    Message msg = Message{m_id, m_id, payload};
    m_network.send(msg);
    if (m_thread.joinable()) {
      m_thread.join();
    }
  }
};

void Node::send_to(uint32_t id, Block payload) {
  m_network.send(Message{m_id, id, payload});
}

void Node::propose_block(Block block) {
  // We store our own block and vote on it
  pre_prepared_recent = std::make_unique<Block>(block);
  std::set<uint32_t> &current_votes = prepare_votes[block];
  current_votes.insert(m_id);

  broadcast(block);
}

void Node::broadcast(Block payload) { m_network.broadcast(m_id, payload); }

uint32_t Node::id() const noexcept { return m_id; }

Block Node::pre_prepare_block(Value value) {

  Block block = Block{
      MessageType::PrePrepare,
      view,
      instance_id,
      value,
  };
  return block;
}

Block Node::prepare_block(Value value) {
  Block block = Block{
      MessageType::Prepare,
      view,
      instance_id,
      value,
  };
  return block;
}

Block Node::commit_block(Value value) {
  Block block = Block{
      MessageType::Commit,
      view,
      instance_id,
      value,
  };
  return block;
}

void Node::print_message(const Message &m) {
  std::osyncstream bout(std::cout);
  bout << "Node: " << m_id << ' ' << m << std::endl;
};

void Node::print_string(const std::string str) {
  std::osyncstream bout(std::cout);
  bout << "Node: " << m_id << ' ' << str << std::endl;
};

uint8_t Node::treat_message(Message msg) {
  Block block = msg.block;
  switch (block.type) {
  case MessageType::PrePrepare: {
    // CHECK
    if (msg.from != block.view) {
      print_string("The pre prepare message didn't come from the leader");
      return 1;
    }
    if (block.instance_id != instance_id) {
      print_string("Not aggreeing on same view");
      return 1;
    }
    if (pre_prepared_recent && *pre_prepared_recent == block) {
      print_string("Already seen this pre_prepare block");
      return 1;
    }

    // Passed all checks so prepare phase
    // We change the most recent pre prepare block we've seen
    pre_prepared_recent = std::make_unique<Block>(block);
    Block replica_prepare_block = prepare_block(block.value);
    // We record our own vote
    auto &prepare_voters = this->prepare_votes[block];
    prepare_voters.insert(m_id);
    // We broadcast to whole network and count our own vote
    //  std::this_thread::sleep_for(std::chrono::milliseconds(10));
    broadcast(replica_prepare_block);
    return 0;
  };
  case MessageType::Prepare: {
    // Check that we are agreeing on same view
    if (block.view != view) {
      print_string("Invalid view on block");
      return 2;
    }
    // check that we have same instance id
    if (block.instance_id != instance_id) {
      print_string("Invalid instance id");
      return 2;
    }

    // Check if we've already received this value
    std::set<uint32_t> &current_votes = prepare_votes[block];
    if (current_votes.find(msg.from) != current_votes.end()) {
      print_string("Already received prepare message from this node");
      return 1;
    }
    // Check that the block prepared matches the block the leader produced (pre
    // prepared)
    if (pre_prepared_recent && *pre_prepared_recent != block) {
      print_string("Still havent seen block from leader: can't prepare");
      // Add that message to treat later
      pending_prepare_messages[block].push_back(msg);
      return 1;
    }
    // We merge the current block and the pending blocks to treat
    //

    auto it = pending_prepare_messages.find(block);
    if (it == pending_prepare_messages.end()) {
      pending_prepare_messages[block] = {msg};
    } else {
      it->second.push_back(msg);
    }
    auto it2 = pending_prepare_messages.find(block);
    if (it2 != pending_prepare_messages.end()) {
      auto pending = std::move(it2->second);
      pending_prepare_messages.erase(it2);

      for (const Message &m : pending) {

        if (current_votes.find(m.from) != current_votes.end()) {
          continue;
        }
        // Record the vote
        current_votes.insert(m.from);
        std::string vote_size_str =
            "Current vote size: " + std::to_string(current_votes.size()) +
            " quorum size: " + std::to_string(m_network.quorum_size());
        print_string(vote_size_str);

        if (current_votes.size() == m_network.quorum_size()) {
          //We only have to do this once
          print_string("broadcasting committing block");
          Block bl = m.block;
          // We start commit phase and record our vote than others
          auto &commit_voters = commit_votes[bl];
          // We insert our vote in the set (unique)
          Block replica_commit_block = commit_block(bl.value);
          commit_voters.insert(m_id);
          // we broadcast our commit to other nodes
          broadcast(replica_commit_block);

          // if (current_votes.size() == m_network.quorum_size()) {
          //   // We only broadcast once
          //   commit_voters.insert(m_id);
          //   broadcast(replica_commit_block);
          // }
        };
      }
      // We inserted all necessary messages
    }
    return 0;
  };

  case MessageType::Commit: {

    if (block.view != view) {
      print_string("Invalid view on block");
      return 2;
    }
    // check that we have same instance id
    if (block.instance_id != instance_id) {
      print_string("Invalid instance id");
      return 2;
    }

    // Check if we've already received this value
    std::set<uint32_t> &current_votes = commit_votes[block];
    if (current_votes.find(msg.from) != current_votes.end()) {
      print_string("Already received prepare message from this node");
      return 1;
    }
    // Check that the block prepared matches the block the leader produced (pre
    // prepared)
    if (pre_prepared_recent && *pre_prepared_recent != block) {
      print_string("Still havent seen block from leader: can't treat commit");
      pending_commit_messages[block].push_back(msg);
      return 1;
    }

    // Build the messages we will iterate through (the one received and also
    // the)
    auto it = pending_commit_messages.find(block);
    if (it == pending_commit_messages.end()) {
      pending_commit_messages[block] = {msg};
    } else {
      it->second.push_back(msg);
    }
    auto it2 = pending_commit_messages.find(block);
    if (it2 != pending_commit_messages.end()) {
      auto pending = std::move(it2->second);
      pending_commit_messages.erase(it2);
      for (const Message &m : pending) {
        if (current_votes.find(m.from) != current_votes.end()) {
          continue;
        }

        current_votes.insert(m_id);

        std::string vote_size_str =
            "Current vote size: " + std::to_string(current_votes.size()) +
            " quorum size: " + std::to_string(m_network.quorum_size());
        print_string(vote_size_str);

        if (current_votes.size() >= m_network.quorum_size()) {
          std::string commited_block_string =
              "Commited block: " + block_string(block);
          print_string(commited_block_string);
          commited_blocks[block.instance_id] = block;
        }
      }
    }

    // Record the vote
    current_votes.insert(msg.from);
    if (current_votes.size() >= m_network.quorum_size()) {
      std::string str =
          "Commiting block instance" + std::to_string(block.instance_id);
      print_string(str);
    }
    return 0;
  };
  case MessageType::Shutdown: {
    std::cerr << "Message shutdown shouldn't happen" << std::endl;
    return 0;
  }
  }
}

void Node::treat_message_queue() {
  bool progress = true;

  while (!untreated.empty() && progress) {
    progress = false;
    std::size_t n = untreated.size();

    for (std::size_t k = 0; k < n; ++k) {
      Message msg = std::move(untreated.front());
      untreated.pop_front();

      if (treat_message(msg) == 0) { // 0 = success in your code
        // message fully handled, don't requeue
        print_string("Treated msg");
        progress = true;
      } else {
        // not yet treatable -> put it back at the end
        print_string("Failed treating msg");
        untreated.push_back(std::move(msg));
      }
    }
  }
}

void Node::start() {
  m_thread = std::thread(&Node::run, this);
}
void Node::run() {
  // print_string("Running...");
  while (true) {
    // Wait to receive a message
    Message msg = m_network.recv(m_id);
    if (!m_running.load()) {
      // If we've stopped running we break;
      break;
    }
    if (msg.block.type == MessageType::Shutdown) {
      break;
    } else {
      untreated.push_front(msg);
      // print_message(msg);
      treat_message_queue();
      // treat_message(msg);
    }
    print_message(msg);
  }
  
  std::string bl_str ="Commited block: " +  block_string(commited_blocks[0]);
  print_string(bl_str);
  print_string("Shutting down...");
  
}

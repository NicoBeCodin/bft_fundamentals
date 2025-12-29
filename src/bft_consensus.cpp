#include "../include/bft_consensus.h"
#include "../include/node.h"

BFTConsensus::BFTConsensus(uint32_t leader_id)
  : leader_(leader_id), slot_(0) {}

void BFTConsensus::on_start(Node& node) {
  if (is_leader(node.id())) {
    Value v{67};
    propose(v, node);
  }
}

uint8_t BFTConsensus::handle_message(const Message& msg, Node& node) {
  switch (msg.block.type) {
    case MessageType::PrePrepare: return handle_pre_prepare(msg, node);
    case MessageType::Prepare:    return handle_prepare(msg, node);
    case MessageType::Commit:     return handle_commit(msg, node);
    case MessageType::Shutdown:   return 0;
    default:                      return 2;
  }
}

Block BFTConsensus::make_block(MessageType t, const Value& v) const {
  return Block{ t, leader_, slot_, v };
}

void BFTConsensus::propose(const Value& v, Node& node) {
  if (!is_leader(node.id())) return;
  Block pre = make_block(MessageType::PrePrepare, v);
  pre_prepared_recent_ = std::make_unique<Block>(pre);
  node.broadcast(pre);
}

uint8_t BFTConsensus::handle_pre_prepare(const Message& msg, Node& node) {
  Block block = msg.block;

  if (msg.from != block.view) {
    node.print_string("PrePrepare didn't come from leader");
    return 2;
  }
  if (block.instance_id != slot_) {
    node.print_string("PrePrepare has wrong slot/instance_id");
    return 1;
  }
  if (pre_prepared_recent_ && *pre_prepared_recent_ == block) {
    node.print_string("Already saw this PrePrepare");
    return 1;
  }

  pre_prepared_recent_ = std::make_unique<Block>(block);

  // Vote prepare for this block (count own vote)
  auto& voters = prepare_votes_[block];
  voters.insert(node.id());

  Block prepare = make_block(MessageType::Prepare, block.value);
  node.broadcast(prepare);
  return 0;
}

uint8_t BFTConsensus::handle_prepare(const Message& msg, Node& node) {
  Block block = msg.block;

  if (block.view != leader_) {
    node.print_string("Prepare has invalid leader/view");
    return 2;
  }
  if (block.instance_id != slot_) {
    node.print_string("Prepare has invalid slot");
    return 1;
  }

  auto& current_votes = prepare_votes_[block];
  if (current_votes.find(msg.from) != current_votes.end()) {
    node.print_string("Duplicate Prepare from same node");
    return 1;
  }

  // Require matching pre-prepare before counting prepares (PBFT dependency)
  if (!pre_prepared_recent_ || *pre_prepared_recent_ != block) {
    node.print_string("Haven't seen leader block yet, queue Prepare");
    pending_prepare_messages_[block].push_back(msg);
    return 1;
  }

  // Merge msg into pending list and process as a batch
  pending_prepare_messages_[block].push_back(msg);
  auto pending = std::move(pending_prepare_messages_[block]);
  pending_prepare_messages_.erase(block);

  for (const auto& m : pending) {
    if (current_votes.find(m.from) != current_votes.end()) continue;
    current_votes.insert(m.from);

    node.print_string("Prepare vote size: " + std::to_string(current_votes.size()) +
                      " quorum: " + std::to_string(node.get_quorum_size()));

    if (current_votes.size() == node.get_quorum_size()) {
      // Enter commit phase (count own vote)
      auto& commit_voters = commit_votes_[block];
      commit_voters.insert(node.id());

      node.print_string("Broadcasting Commit");
      Block commit = make_block(MessageType::Commit, block.value);
      node.broadcast(commit);
    }
  }

  return 0;
}

uint8_t BFTConsensus::handle_commit(const Message& msg, Node& node) {
  Block block = msg.block;

  if (block.view != leader_) {
    node.print_string("Commit has invalid leader/view");
    return 2;
  }
  if (block.instance_id != slot_) {
    node.print_string("Commit has invalid slot");
    return 1;
  }

  // Must match leader's pre-prepare
  if (!pre_prepared_recent_ || *pre_prepared_recent_ != block) {
    node.print_string("Haven't seen leader block yet, queue Commit");
    pending_commit_messages_[block].push_back(msg);
    return 1;
  }

  auto& current_votes = commit_votes_[block];
  if (current_votes.find(msg.from) != current_votes.end()) {
    node.print_string("Duplicate Commit from same node");
    return 1;
  }

  // Merge msg into pending list and process as a batch
  pending_commit_messages_[block].push_back(msg);
  auto pending = std::move(pending_commit_messages_[block]);
  pending_commit_messages_.erase(block);

  for (const auto& m : pending) {
    if (current_votes.find(m.from) != current_votes.end()) continue;
    current_votes.insert(m.from);

    node.print_string("Commit vote size: " + std::to_string(current_votes.size()) +
                      " quorum: " + std::to_string(node.get_quorum_size()));

    if (current_votes.size() >= node.get_quorum_size()) {
      // commit exactly once
      if (commited_blocks_.find(block.instance_id) == commited_blocks_.end()) {
        node.print_string("Committed block: " + block_string(block));
        commited_blocks_[block.instance_id] = block;
      }
    }
  }

  return 0;
}

#include "../include/bft_consensus.h"
#include "../include/node.h"

BFTConsensus::BFTConsensus(uint32_t leader_id)
  : leader_(leader_id), slot_(0) {}

void BFTConsensus::on_start(Node& node) {
  if (is_leader(node.id())) {
    BlockData v{67};
    propose(v, node);
  }
}

uint8_t BFTConsensus::handle_message(const P2PMessage& msg, Node& node) {
  switch (msg.proposal.phase) {
    case BFTPhase::PrePrepare: return handle_pre_prepare(msg, node);
    case BFTPhase::Prepare:    return handle_prepare(msg, node);
    case BFTPhase::Commit:     return handle_commit(msg, node);
    case BFTPhase::Shutdown:   return 0;
    default:                      return 2;
  }
}
// static bool same_proposal(const BFTProposal& a, const BFTProposal& b){
//   return a.view == b.view && a.slot == b.slot && a.value == b.value;
// }

BFTProposal BFTConsensus::make_block_phase_proposal(const BFTPhase t, const BlockData& v) const {
  return BFTProposal{ t, leader_, slot_, v };
}

void BFTConsensus::propose(const BlockData& v, Node& node) {
  if (!is_leader(node.id())) return;
  BFTProposal pre = make_block_phase_proposal(BFTPhase::PrePrepare, v);
  ProposalId proposal_id = proposal_key(pre);
  pre_prepared_recent_ = std::make_unique<ProposalId>(proposal_id);
  node.broadcast(pre);
}

uint8_t BFTConsensus::handle_pre_prepare(const P2PMessage& msg, Node& node) {
  BFTProposal proposal = msg.proposal;
  ProposalId key = proposal_key(proposal);

  if (msg.from != key.view) {
    node.print_string("PrePrepare didn't come from leader");
    return 2;
  }
  if (key.slot != slot_) {
    node.print_string("PrePrepare has wrong slot/instance_id");
    return 1;
  }
  if (pre_prepared_recent_ && *pre_prepared_recent_ == key ) {
    node.print_string("Already saw this PrePrepare");
    return 1;
  }

  pre_prepared_recent_ = std::make_unique<ProposalId>(key);

  // Vote prepare for this block (count own vote)
  auto& voters = prepare_votes_[key];
  voters.insert(node.id());

  BFTProposal prepare = make_block_phase_proposal(BFTPhase::Prepare, key.value);
  node.broadcast(prepare);
  return 0;
}

uint8_t BFTConsensus::handle_prepare(const P2PMessage& msg, Node& node) {
  BFTProposal block = msg.proposal;
  ProposalId key = proposal_key(block);

  if (block.view != leader_) {
    node.print_string("Prepare has invalid leader/view");
    return 2;
  }
  if (block.slot != slot_) {
    node.print_string("Prepare has invalid slot");
    return 1;
  }

  auto& current_prepare_votes = prepare_votes_[key];
  if (current_prepare_votes.find(msg.from) != current_prepare_votes.end()) {
    node.print_string("Duplicate Prepare from same node");
    return 1;
  }

  // Require matching pre-prepare before counting prepares (PBFT dependency)
  if (!pre_prepared_recent_ || !(*pre_prepared_recent_==key)) {
    node.print_string("Haven't seen leader block yet, queue Prepare");
    pending_prepare_messages_[key].push_back(msg);
    return 1;
  }

  // Merge msg into pending list and process as a batch
  pending_prepare_messages_[key].push_back(msg);
  auto pending = std::move(pending_prepare_messages_[key]);
  pending_prepare_messages_.erase(key);

  for (const auto& m : pending) {
    if (current_prepare_votes.find(m.from) != current_prepare_votes.end()) continue;
    current_prepare_votes.insert(m.from);

    node.print_string("Prepare vote size: " + std::to_string(current_prepare_votes.size()) +
                      " quorum: " + std::to_string(node.get_quorum_size()));

    if (current_prepare_votes.size() == quorum_size(node)) {
      // Enter commit phase (count own vote)
      auto& commit_voters = commit_votes_[key];
      commit_voters.insert(node.id());

      node.print_string("Broadcasting Commit");
      BFTProposal commit = make_block_phase_proposal(BFTPhase::Commit, block.value);
      node.broadcast(commit);
    }
  }

  return 0;
}

uint8_t BFTConsensus::handle_commit(const P2PMessage& msg, Node& node) {
  BFTProposal block = msg.proposal;
  ProposalId key = proposal_key(msg.proposal);
  
  if (block.view != leader_) {
    node.print_string("Commit has invalid leader/view");
    return 2;
  }
  if (block.slot != slot_) {
    node.print_string("Commit has invalid slot");
    return 1;
  }

  // Must match leader's pre-prepare
  if (!pre_prepared_recent_ || !(*pre_prepared_recent_ == key)) {
    node.print_string("Haven't seen leader block yet, queue Commit");
    pending_commit_messages_[key].push_back(msg);
    return 1;
  }

  auto& current_votes = commit_votes_[key];
  if (current_votes.find(msg.from) != current_votes.end()) {
    node.print_string("Duplicate Commit from same node");
    return 1;
  }

  // Merge msg into pending list and process as a batch
  pending_commit_messages_[key].push_back(msg);
  auto pending = std::move(pending_commit_messages_[key]);
  pending_commit_messages_.erase(key);

  for (const auto& m : pending) {
    if (current_votes.find(m.from) != current_votes.end()) continue;
    current_votes.insert(m.from);

    node.print_string("Commit vote size: " + std::to_string(current_votes.size()) +
                      " quorum: " + std::to_string(node.get_quorum_size()));

    if (current_votes.size() >= quorum_size(node)) {
      // commit exactly once
      if (commited_blocks_.find(block.slot) == commited_blocks_.end()) {
        node.print_string("Committed block: " + block_string(block));
        commited_blocks_[key.slot] = key;
      }
    }
  }

  return 0;
}

size_t BFTConsensus::quorum_size(Node& node) const {
  size_t f= (node.get_cluster_size() -1)/ 3;
  return 2*f +1;
}

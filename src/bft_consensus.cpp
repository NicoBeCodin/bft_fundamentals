#include "../include/bft_consensus.h"
#include "../include/node.h"

BFTConsensus::BFTConsensus(uint32_t leader_id)
  : leader_(leader_id), slot_(0) {
    Block initial_block;
    commited_blocks_chain.push_back(initial_block);
  }

void BFTConsensus::on_start(Node& node) {
  if (is_leader(node.id())) {
    Block v{67};
    propose(v, node);
  }
}

Block BFTConsensus::last_commited_block(){
  return commited_blocks_chain.back();
}

uint8_t BFTConsensus::handle_message(const P2PMessage& msg, Node& node) {
  switch (msg.msg_kind) {
    case MessageKind::BlockProposalMessage: return handle_block_proposal(msg, node);
    case MessageKind::BFTVoteMessage: return handle_bft_vote_message(msg, node);
    case MessageKind::TxGossipMessage: throw std::runtime_error("Unimplemented TxGossip");
    // case BFTPhase::Shutdown:   return 0;
    default:                      return 2;
  }
}

uint8_t BFTConsensus::handle_bft_vote_message(const P2PMessage& msg, Node& node) {
  BFTVote vote;
  vote.from_payload(msg.payload);
  if (vote.phase == BFTPhase::Prepare){
    return handle_prepare(msg, node);
  } else {
    return handle_commit(msg, node);
  }
}
// static bool same_proposal(const BFTProposal& a, const BFTProposal& b){
//   return a.view == b.view && a.slot == b.slot && a.value == b.value;
// }

BlockProposal BFTConsensus::make_block_proposal(const Block& block) const {
  return BlockProposal{ leader_, slot_+1, block }; //For next slot
}

void BFTConsensus::propose(const Block& block, Node& node) {
  if (!is_leader(node.id())) return;
  BlockProposal pre = make_block_proposal(block);
  recent_proposed_block = std::make_unique<BlockProposal>(pre);
  node.broadcast(pre);
}

BFTVote BFTConsensus::make_bft_vote(Node& n, BlockProposal& bp, BFTPhase phase){
  BlockHash bh = bp.block.hash_block_data_to_bytes();
  std::vector<uint32_t> vote_vec{n.id()};
  return BFTVote {
    leader_,
    slot_,
    vote_vec,
    phase,
    bh
};
  
}

uint8_t BFTConsensus::handle_block_proposal(const P2PMessage& msg, Node& node) {
  BlockProposal proposed_block;
  proposed_block.from_payload(msg.payload);

  //Could relax this in the future
  if (msg.from != leader_) {
    node.print_string("BlockProposal didn't come from leader");
    return 2;
  }

  //Not agreeing on slot
  if (proposed_block.slot != slot_) {
    node.print_string("PrePrepare has wrong slot/instance_id");
    return 1;
  }

  //Avoid doing it twice
  if (recent_proposed_block && *recent_proposed_block == proposed_block ) {
    node.print_string("Already saw this PrePrepare");
    return 1;
  }
  if (recent_proposed_block && recent_proposed_block->block.height != commited_blocks_chain.back().height+1){
    node.print_string("Proposed block isn't higher than most recent commited");
    return 1;
  }

  recent_proposed_block = std::make_unique<BlockProposal>(proposed_block);
  BlockHash bh = proposed_block.block.hash_block_data_to_bytes();

  // Vote prepare for this block (count own vote)
  auto& voters = prepare_votes_[bh];
  voters.insert(node.id());

  BFTVote prepare = make_bft_vote(node, proposed_block, BFTPhase::Prepare);
  node.broadcast(prepare);
  return 0;
}

uint8_t BFTConsensus::handle_prepare(const P2PMessage& msg, Node& node) {
  BFTVote prepare_vote;
  prepare_vote =  *prepare_vote.from_payload(msg.payload);
  BlockHash recent_bh = recent_proposed_block->block.hash_block_data_to_bytes();
  

  if (prepare_vote.leader != leader_) {
    node.print_string("Prepare has invalid leader/view");
    return 2;
  }
  if (prepare_vote.slot != slot_) {
    node.print_string("Prepare has invalid slot");
    return 1;
  }

  auto& current_prepare_votes = prepare_votes_[recent_bh];
  if (current_prepare_votes.find(msg.from) != current_prepare_votes.end()) {
    node.print_string("Duplicate Prepare from same node");
    return 1;
  }

  // Require matching pre-prepare before counting prepares (PBFT dependency)
  if (!recent_proposed_block || !(prepare_vote.blockhash==recent_bh)) {
    node.print_string("Haven't seen leader block yet, queue Prepare");
    pending_prepare_messages_[recent_bh].push_back(prepare_vote);
    return 1;
  }

  // Merge msg into pending list and process as a batch
  pending_prepare_messages_[prepare_vote.blockhash].push_back(prepare_vote);
  auto pending = std::move(pending_prepare_messages_[prepare_vote.blockhash]);
  pending_prepare_messages_.erase(prepare_vote.blockhash);

  for (const auto& v : pending) {
    if (current_prepare_votes.find(v.votes[0]) != current_prepare_votes.end()) continue; //At the moment the vec only has 
    current_prepare_votes.insert(v.votes[0]);

    node.print_string("Prepare vote size: " + std::to_string(current_prepare_votes.size()) +
                      " quorum: " + std::to_string(node.get_quorum_size()));

    if (current_prepare_votes.size() == quorum_size(node)) {
      // Enter commit phase (count own vote)
      auto& commit_voters = commit_votes_[prepare_vote.blockhash];
      commit_voters.insert(node.id());

      node.print_string("Broadcasting Commit");
      BFTVote commit = make_bft_vote(node, *recent_proposed_block, BFTPhase::Commit);
      node.broadcast(commit);
    }
  }

  return 0;
}

uint8_t BFTConsensus::handle_commit(const P2PMessage& msg, Node& node) {
  BFTVote bft_vote = *BFTVote::from_payload(msg.payload);
  BlockHash key = bft_vote.blockhash;
  
  
  if (bft_vote.leader != leader_) {
    node.print_string("Commit has invalid leader/view");
    return 2;
  }
  if (bft_vote.slot != slot_) {
    node.print_string("Commit has invalid slot");
    return 1;
  }

  // Must match leader's pre-prepare
  if (!recent_proposed_block || !(recent_proposed_block->block.hash_block_data_to_bytes() == key)) {
    node.print_string("Haven't seen leader block yet, queue Commit");
    pending_commit_messages_[key].push_back(bft_vote);
    return 1;
  }

  //This shall be modified in the future to avoid double voting
  auto& current_votes = commit_votes_[key];
  if (current_votes.find(bft_vote.votes[0]) != current_votes.end()) {
    node.print_string("Duplicate Commit from same node");
    return 1;
  }

  // Merge msg into pending list and process as a batch
  pending_commit_messages_[key].push_back(bft_vote);
  auto pending = std::move(pending_commit_messages_[key]);
  pending_commit_messages_.erase(key);

  for (const auto& v : pending) {
    if (current_votes.find(v.votes[0]) != current_votes.end()) continue;
    current_votes.insert(v.votes[0]);

    node.print_string("Commit vote size: " + std::to_string(current_votes.size()) +
                      " quorum: " + std::to_string(node.get_quorum_size()));

    if (current_votes.size() >= quorum_size(node)) {
      //Ensure we haven't commited it before
      if (!(commited_blocks_chain.back() == recent_proposed_block->block)){
        //Commit block to chain
        commited_blocks_chain.push_back(recent_proposed_block->block);
        node.print_string("Commiting block to vector");
      }
    }
  }

  return 0;
}


uint32_t BFTConsensus::get_leader(){
  return leader_;
}
uint32_t BFTConsensus::get_slot(){
  return slot_;
}

size_t BFTConsensus::quorum_size(Node& node) const {
  size_t f= (node.get_cluster_size() -1)/ 3;
  return 2*f +1;
}

#pragma once
#include "consensus.h"
#include "message.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

class BFTConsensus : public IConsensus {
public:
  explicit BFTConsensus(uint32_t leader_id = 0);

  void on_start(Node& node) override;
  uint8_t handle_message(const P2PMessage& msg, Node& node) override;
  Block last_commited_block() override;
  uint32_t get_leader() override;
  uint32_t get_slot() override;
  
  BlockProposal create_random_block(uint32_t, uint32_t) override;
  void insert_proposed_block(BlockProposal) override;

private:
  bool is_leader(uint32_t node_id) const { return node_id == leader_; }

  size_t quorum_size(Node& node) const;
  BlockProposal make_block_proposal(const Block& v) const;
  BFTVote make_bft_vote(Node& node, BlockProposal& b, BFTPhase phase);
  void propose(const Block& v, Node& node);
  uint8_t handle_block_proposal(const P2PMessage& msg, Node& node);
  uint8_t handle_bft_vote_message(const P2PMessage& msg, Node& node);
  uint8_t handle_prepare(const P2PMessage& msg, Node& node);
  uint8_t handle_commit(const P2PMessage& msg, Node& node);

  uint32_t leader_ = 0;
  uint32_t slot_ = 0;
  std::unique_ptr<BlockProposal> recent_proposed_block{};
  

  std::map<BlockHash, std::set<uint32_t>> prepare_votes_;
  std::map<BlockHash, std::set<uint32_t>> commit_votes_;
  
  std::vector<Block> commited_blocks_chain;
  // std::map<uint32_t, BlockProposal> commited_blocks_; //This could just be a vector

  std::map<BlockHash, std::vector<BFTVote>> pending_prepare_messages_;
  std::map<BlockHash, std::vector<BFTVote>> pending_commit_messages_;
};

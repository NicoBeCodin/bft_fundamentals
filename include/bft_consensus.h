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

private:
  bool is_leader(uint32_t node_id) const { return node_id == leader_; }

  size_t quorum_size(Node& node) const;
  BFTProposal make_block_phase_proposal(BFTPhase t, const BlockData& v) const;
  void propose(const BlockData& v, Node& node);

  uint8_t handle_pre_prepare(const P2PMessage& msg, Node& node);
  uint8_t handle_prepare(const P2PMessage& msg, Node& node);
  uint8_t handle_commit(const P2PMessage& msg, Node& node);

private:
  uint32_t leader_ = 0;
  uint32_t slot_ = 0;

  std::unique_ptr<ProposalId> pre_prepared_recent_;

  std::map<ProposalId, std::set<uint32_t>> prepare_votes_;
  std::map<ProposalId, std::set<uint32_t>> commit_votes_;

  std::map<uint32_t, ProposalId> commited_blocks_;

  std::map<ProposalId, std::vector<P2PMessage>> pending_prepare_messages_;
  std::map<ProposalId, std::vector<P2PMessage>> pending_commit_messages_;
};

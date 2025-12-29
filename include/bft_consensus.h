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
  uint8_t handle_message(const Message& msg, Node& node) override;

private:
  bool is_leader(uint32_t node_id) const { return node_id == leader_; }

  Block make_block(MessageType t, const Value& v) const;
  void propose(const Value& v, Node& node);

  uint8_t handle_pre_prepare(const Message& msg, Node& node);
  uint8_t handle_prepare(const Message& msg, Node& node);
  uint8_t handle_commit(const Message& msg, Node& node);

private:
  uint32_t leader_ = 0;
  uint32_t slot_ = 0;

  std::unique_ptr<Block> pre_prepared_recent_;

  std::map<Block, std::set<uint32_t>> prepare_votes_;
  std::map<Block, std::set<uint32_t>> commit_votes_;

  std::map<uint32_t, Block> commited_blocks_;

  std::map<Block, std::vector<Message>> pending_prepare_messages_;
  std::map<Block, std::vector<Message>> pending_commit_messages_;
};

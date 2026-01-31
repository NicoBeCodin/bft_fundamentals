#pragma once
#include "../include/message.h"
#include <cstring>
#include <openssl/sha.h>

const char *to_string(BFTPhase t) {
  switch (t) {
  // case BFTPhase::PrePrepare: return "PRE_PREPARE";
  case BFTPhase::Prepare:
    return "PREPARE";
  case BFTPhase::Commit:
    return "COMMIT";
    // case BFTPhase::Shutdown:   return "SHUTDOWN";
    // default:                      return "UNKNOWN";
  }
  return "ERROR_TYPE";
}

std::array<uint8_t, 32> sha256_bytes(const std::string &s) {
  std::array<uint8_t, 32> out{};
  SHA256(reinterpret_cast<const unsigned char *>(s.data()), s.size(),
         out.data());
  return out; // returns by value (safe)
}

BlockHash Block::hash_block_data_to_bytes() {
  std::string height_string = std::to_string(height);

  std::string previous_hash_string(
      reinterpret_cast<const char *>(previous_hash.data()),
      previous_hash.size());

  return sha256_bytes(height_string + previous_hash_string + transactions);
}

std::vector<uint8_t> Block::to_payload() const {
  Writer w;
  w.u32(height);
  w.write_hash(previous_hash);
  w.write_str(transactions);
  return w.buffer;
}

std::optional<Block> Block::from_payload(const std::vector<uint8_t> pl) {
  Reader r(pl);
  Block block;
  auto height = r.read_u32();
  auto bh = r.read_blockhash();
  auto txs = r.read_string();
  if (!height | !bh | !txs)
    return std::nullopt;
  block.height = *height;
  block.previous_hash = *bh;
  block.transactions = *txs;
  return block;
}

std::vector<uint8_t> BlockProposal::to_payload() const {
  Writer w;
  w.u32(view);
  w.u32(slot);
  std::vector<uint8_t> block_payload = block.to_payload();
  w.write_payload(block_payload);
  return w.buffer;
}

std::optional<BlockProposal>
BlockProposal::from_payload(const std::vector<uint8_t> &pl) {
  Reader r(pl);
  BlockProposal bp;
  auto view = r.read_u32();
  auto slot = r.read_u32();

  Block bl;
  std::vector<uint8_t> block_vector(r.n);
  memcpy(block_vector.data(), r.p, r.n);
  auto block_opt = Block::from_payload(block_vector);

  if (!view || !slot || !block_opt)
    return std::nullopt;

  bp.view = *view;
  bp.slot = *slot;
  bp.block = *block_opt;
  return std::optional<BlockProposal>(bp);
}

std::vector<uint8_t> BFTVote::to_payload() const {
  Writer w;
  w.u32(leader);
  w.u32(slot);
  w.write_u32_vector(votes);
  w.write_phase(phase);
  w.write_hash(blockhash);
  return w.buffer;
}

std::optional<BFTVote> BFTVote::from_payload(const std::vector<uint8_t> &pl) {
  Reader r(pl);
  BFTVote bft_vote;
  auto view = r.read_u32();
  auto slot = r.read_u32();
  auto votes = r.read_u32_vector();
  auto phase = r.read_phase();
  auto bh = r.read_blockhash();
  if (!view || !slot || !votes || !phase || !bh)
    return std::nullopt;
  bft_vote.leader = *view;
  bft_vote.slot = *slot;
  bft_vote.votes = *votes;
  bft_vote.phase = *phase;
  bft_vote.blockhash = *bh;
  return std::optional<BFTVote>(bft_vote);
}

std::string to_hex(const uint8_t *data, size_t n) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < n; ++i)
    oss << std::setw(2) << static_cast<int>(data[i]);
  return oss.str();
}

std::string message_kind_to_string(MessageKind m) {
  switch (m) {
  case MessageKind::BFTVoteMessage:
    return "BFTVote";
  case MessageKind::BlockProposalMessage:
    return "BlockProposal";
  case MessageKind::TxGossipMessage:
    return "TxGossip";
  }
}

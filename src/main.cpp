#include "../include/node.h"
#include "../include/tcp_transport.h"
#include "../include/bft_consensus.h"
#include <cstdlib>
#include <iostream>
#include <sstream>

static uint32_t env_u32(const char* k, uint32_t def) {
  const char* v = std::getenv(k);
  if (!v) return def;
  try { return static_cast<uint32_t>(std::stoul(v)); } catch (...) { return def; }
}

static uint16_t env_u16(const char* k, uint16_t def) {
  const char* v = std::getenv(k);
  if (!v) return def;
  try { return static_cast<uint16_t>(std::stoul(v)); } catch (...) { return def; }
}

static std::vector<PeerInfo> parse_peers(const std::string& s) {
  // Format: id@host:port,id@host:port
  std::vector<PeerInfo> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (item.empty()) continue;
    auto at = item.find('@');
    auto colon = item.rfind(':');
    if (at == std::string::npos || colon == std::string::npos || colon < at) continue;

    uint32_t id = 0;
    try { id = static_cast<uint32_t>(std::stoul(item.substr(0, at))); } catch (...) { continue; }

    std::string host = item.substr(at + 1, colon - (at + 1));
    uint16_t port = 0;
    try { port = static_cast<uint16_t>(std::stoul(item.substr(colon + 1))); } catch (...) { continue; }

    out.push_back(PeerInfo{id, host, port});
  }
  return out;
}

int main() {
  const uint32_t node_id = env_u32("NODE_ID", 0);
  const uint16_t listen_port = env_u16("LISTEN_PORT", 9000);
  const uint32_t cluster_size = env_u32("CLUSTER_SIZE", 4);

  const char* peers_env = std::getenv("PEERS");
  std::string peers_s = peers_env ? std::string(peers_env) : "";
  auto peers = parse_peers(peers_s);

  std::cout << "Starting node_id=" << node_id
            << " listen_port=" << listen_port
            << " cluster_size=" << cluster_size
            << " peers=\"" << peers_s << "\"\n";

  // Node deliver callback
  Node* node_ptr = nullptr;

  auto deliver = [&](Message&& m) {
    if (node_ptr) node_ptr->on_receive(std::move(m));
  };

  auto transport = std::make_unique<TcpTransport>(
      node_id, listen_port, peers, static_cast<size_t>(cluster_size), deliver);

  auto consensus = std::make_unique<BFTConsensus>(/*leader_id=*/0);

  Node node(node_id, cluster_size, std::move(transport), std::move(consensus));
  node_ptr = &node;

  node.start();

  // Run forever (Ctrl+C to stop container)
  // In production you'd handle signals to stop cleanly.
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }

  return 0;
}

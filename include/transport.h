#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include <string>

#include "message.h"

struct PeerInfo {
  uint32_t id;
  std::string host;
  uint16_t port;
};

class ITransport {
public:
  using DeliverFn = std::function<void(P2PMessage&&)>;

  virtual ~ITransport() = default;


  //These methods could be useful for tcp transport but at the moment a thread just needs an open port
  virtual void start(uint32_t node_id) = 0;
  virtual void stop() = 0;

  virtual void send(uint32_t to, const BFTProposal& msg) = 0;
  virtual void broadcast(const BFTProposal& msg) = 0;
  virtual P2PMessage recv() = 0;

  virtual size_t cluster_size() const = 0;
};


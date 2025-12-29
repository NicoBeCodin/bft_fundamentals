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
  using DeliverFn = std::function<void(Message&&)>;

  virtual ~ITransport() = default;

  virtual void start() = 0;
  virtual void stop() = 0;

  virtual void send(uint32_t to, const Message& msg) = 0;
  virtual void broadcast(const Message& msg) = 0;

  virtual size_t cluster_size() const = 0;
};


#pragma once
#include "transport.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class TcpTransport : public ITransport {
public:
  TcpTransport(uint32_t self_id,
               uint16_t listen_port,
               std::vector<PeerInfo> peers,
               size_t cluster_size,
               DeliverFn deliver);

  ~TcpTransport() override { stop(); }

  void start() override;
  void stop() override;

  void send(uint32_t to, const Message& msg) override;
  void broadcast(const Message& msg) override;

  size_t cluster_size() const override { return cluster_size_; }

private:
  struct Conn {
    int fd = -1;
    uint32_t peer_id = UINT32_MAX;
    std::thread reader;
  };

  void accept_loop();
  void connect_loop(PeerInfo peer);

  void reader_loop(int fd, uint32_t peer_id);
  void close_fd(int fd);

  // framing helpers
  static bool write_all(int fd, const void* data, size_t len);
  static bool read_all(int fd, void* data, size_t len);

  static bool send_handshake(int fd, uint32_t self_id);
  static bool recv_handshake(int fd, uint32_t& peer_id_out);

  static bool send_message_frame(int fd, const Message& msg);
  static bool recv_message_frame(int fd, Message& msg_out);

private:
  uint32_t self_id_;
  uint16_t listen_port_;
  std::vector<PeerInfo> peers_;
  size_t cluster_size_;
  DeliverFn deliver_;

  std::atomic<bool> running_{false};

  int server_fd_ = -1;
  std::thread accept_thread_;

  // peer_id -> connection
  std::mutex conns_mtx_;
  std::unordered_map<uint32_t, Conn> conns_;
};


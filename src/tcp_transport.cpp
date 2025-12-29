#include "../include/tcp_transport.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

static int create_listen_socket(uint16_t port) {
  int s = ::socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) throw std::runtime_error("socket() failed");

  int yes = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(s);
    throw std::runtime_error(std::string("bind() failed: ") + std::strerror(errno));
  }
  if (::listen(s, 64) < 0) {
    ::close(s);
    throw std::runtime_error("listen() failed");
  }
  return s;
}

static int connect_tcp(const std::string& host, uint16_t port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* res = nullptr;
  const std::string port_s = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_s.c_str(), &hints, &res) != 0) return -1;

  int fd = -1;
  for (auto* p = res; p; p = p->ai_next) {
    fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) continue;
    if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
      freeaddrinfo(res);
      return fd;
    }
    ::close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  return -1;
}

TcpTransport::TcpTransport(uint32_t self_id,
                           uint16_t listen_port,
                           std::vector<PeerInfo> peers,
                           size_t cluster_size,
                           DeliverFn deliver)
  : self_id_(self_id),
    listen_port_(listen_port),
    peers_(std::move(peers)),
    cluster_size_(cluster_size),
    deliver_(std::move(deliver)) {}

void TcpTransport::start() {
  if (running_.exchange(true)) return;

  server_fd_ = create_listen_socket(listen_port_);
  accept_thread_ = std::thread(&TcpTransport::accept_loop, this);

  // outbound connect threads (one per peer)
  for (const auto& p : peers_) {
    if (p.id == self_id_) continue;
    std::thread(&TcpTransport::connect_loop, this, p).detach();
  }
}

void TcpTransport::stop() {
  if (!running_.exchange(false)) return;

  if (server_fd_ >= 0) {
    ::shutdown(server_fd_, SHUT_RDWR);
    ::close(server_fd_);
    server_fd_ = -1;
  }

  if (accept_thread_.joinable()) accept_thread_.join();

  std::lock_guard<std::mutex> lk(conns_mtx_);
  for (auto& [peer_id, c] : conns_) {
    if (c.fd >= 0) {
      ::shutdown(c.fd, SHUT_RDWR);
      ::close(c.fd);
      c.fd = -1;
    }
    if (c.reader.joinable()) c.reader.join();
  }
  conns_.clear();
}

void TcpTransport::accept_loop() {
  while (running_.load()) {
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    int fd = ::accept(server_fd_, reinterpret_cast<sockaddr*>(&client), &len);
    if (fd < 0) {
      if (!running_.load()) break;
      continue;
    }

    // handshake: read peer id, then send ours
    uint32_t peer_id = UINT32_MAX;
    if (!recv_handshake(fd, peer_id) || !send_handshake(fd, self_id_)) {
      ::close(fd);
      continue;
    }

    {
      std::lock_guard<std::mutex> lk(conns_mtx_);
      // Replace existing connection if any (simple policy)
      auto it = conns_.find(peer_id);
      if (it != conns_.end()) {
        if (it->second.fd >= 0) {
          ::shutdown(it->second.fd, SHUT_RDWR);
          ::close(it->second.fd);
        }
        if (it->second.reader.joinable()) it->second.reader.join();
        conns_.erase(it);
      }

      Conn c;
      c.fd = fd;
      c.peer_id = peer_id;
      c.reader = std::thread(&TcpTransport::reader_loop, this, fd, peer_id);
      conns_.emplace(peer_id, std::move(c));
    }
  }
}

void TcpTransport::connect_loop(PeerInfo peer) {
  while (running_.load()) {
    int fd = connect_tcp(peer.host, peer.port);
    if (fd < 0) {
      ::usleep(300 * 1000);
      continue;
    }

    // handshake: send ours, read theirs
    if (!send_handshake(fd, self_id_)) {
      ::close(fd);
      continue;
    }
    uint32_t remote_id = UINT32_MAX;
    if (!recv_handshake(fd, remote_id)) {
      ::close(fd);
      continue;
    }

    // If remote_id doesn't match config, we still accept remote_id as truth for simulation.
    {
      std::lock_guard<std::mutex> lk(conns_mtx_);
      auto it = conns_.find(remote_id);
      if (it != conns_.end()) {
        if (it->second.fd >= 0) {
          ::shutdown(it->second.fd, SHUT_RDWR);
          ::close(it->second.fd);
        }
        if (it->second.reader.joinable()) it->second.reader.join();
        conns_.erase(it);
      }

      Conn c;
      c.fd = fd;
      c.peer_id = remote_id;
      c.reader = std::thread(&TcpTransport::reader_loop, this, fd, remote_id);
      conns_.emplace(remote_id, std::move(c));
    }

    // exit connect_loop thread after successful connection;
    // reconnect will be handled if reader_loop exits and the process restarts,
    // or you can add more logic for reconnect on disconnect.
    return;
  }
}

void TcpTransport::reader_loop(int fd, uint32_t peer_id) {
  while (running_.load()) {
    Message msg{};
    if (!recv_message_frame(fd, msg)) break;
    // deliver into Node
    deliver_(std::move(msg));
  }

  // cleanup connection entry
  std::lock_guard<std::mutex> lk(conns_mtx_);
  auto it = conns_.find(peer_id);
  if (it != conns_.end() && it->second.fd == fd) {
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
    it->second.fd = -1;
    // reader thread is this one; do not join itself
  }
}

void TcpTransport::send(uint32_t to, const Message& msg) {
  int fd = -1;
  {
    std::lock_guard<std::mutex> lk(conns_mtx_);
    auto it = conns_.find(to);
    if (it == conns_.end()) return;
    fd = it->second.fd;
  }
  if (fd >= 0) (void)send_message_frame(fd, msg);
}

void TcpTransport::broadcast(const Message& msg) {
  std::vector<int> fds;
  {
    std::lock_guard<std::mutex> lk(conns_mtx_);
    fds.reserve(conns_.size());
    for (auto& [peer_id, c] : conns_) {
      if (c.fd >= 0) fds.push_back(c.fd);
    }
  }
  for (int fd : fds) {
    (void)send_message_frame(fd, msg);
  }
}

// ===== framing / serialization =====
//
// Frame format:
//   uint32_t len_be (payload bytes)
//   payload = 6 * uint32_t fields (24 bytes), all big-endian:
//     from, to, type, view, instance_id, value

bool TcpTransport::write_all(int fd, const void* data, size_t len) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
  size_t left = len;
  while (left > 0) {
    ssize_t n = ::send(fd, p, left, MSG_NOSIGNAL);
    if (n <= 0) return false;
    p += n;
    left -= static_cast<size_t>(n);
  }
  return true;
}

bool TcpTransport::read_all(int fd, void* data, size_t len) {
  uint8_t* p = reinterpret_cast<uint8_t*>(data);
  size_t left = len;
  while (left > 0) {
    ssize_t n = ::recv(fd, p, left, 0);
    if (n <= 0) return false;
    p += n;
    left -= static_cast<size_t>(n);
  }
  return true;
}

bool TcpTransport::send_handshake(int fd, uint32_t self_id) {
  uint32_t be = htonl(self_id);
  return write_all(fd, &be, sizeof(be));
}

bool TcpTransport::recv_handshake(int fd, uint32_t& peer_id_out) {
  uint32_t be = 0;
  if (!read_all(fd, &be, sizeof(be))) return false;
  peer_id_out = ntohl(be);
  return true;
}

bool TcpTransport::send_message_frame(int fd, const Message& msg) {
  uint32_t payload_len = 24;
  uint32_t len_be = htonl(payload_len);

  uint32_t fields[6];
  fields[0] = htonl(msg.from);
  fields[1] = htonl(msg.to);
  fields[2] = htonl(static_cast<uint32_t>(msg.block.type));
  fields[3] = htonl(msg.block.view);
  fields[4] = htonl(msg.block.instance_id);
  fields[5] = htonl(msg.block.value.value);

  return write_all(fd, &len_be, sizeof(len_be)) &&
         write_all(fd, fields, sizeof(fields));
}

bool TcpTransport::recv_message_frame(int fd, Message& msg_out) {
  uint32_t len_be = 0;
  if (!read_all(fd, &len_be, sizeof(len_be))) return false;

  uint32_t len = ntohl(len_be);
  if (len != 24) {
    // For now: reject unexpected frames
    // (You can extend this later with versioning.)
    return false;
  }

  uint32_t fields[6];
  if (!read_all(fd, fields, sizeof(fields))) return false;

  msg_out.from = ntohl(fields[0]);
  msg_out.to   = ntohl(fields[1]);

  const auto type_u = ntohl(fields[2]);
  msg_out.block.type = static_cast<MessageType>(type_u);
  msg_out.block.view = ntohl(fields[3]);
  msg_out.block.instance_id = ntohl(fields[4]);
  msg_out.block.value = Value{ ntohl(fields[5]) };

  return true;
}


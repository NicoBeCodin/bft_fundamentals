#include <chrono>
#include <iostream>
#include <stdlib.h>
#include <thread>
#include "node.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: ./bft n ";
  }
  int n= std::stoi(argv[1]);
  std::cout << "Creating " << n << " nodes " << std::endl;

  Network network(n);
  std::vector<std::unique_ptr<Node>> nodes;
  nodes.reserve(n);
  for (int i = 0; i<n; i++){
    nodes.push_back(std::make_unique<Node>(static_cast<uint32_t>(i), network));
  }

  Value initial_value = Value {67};
  //The leader prepares his value
  Block leader_pre_prepare_payload = nodes[0]->pre_prepare_block(initial_value);
  //He broadcasts his value
  

  nodes[0]->broadcast(leader_pre_prepare_payload);
  
  
  

  
  // nodes[0]->broadcast("I'm the leader");
  // nodes[n-1]->send_to(0, "And i'm the lsat node");


  std::this_thread::sleep_for(std::chrono::seconds(1));
  nodes.clear();

  return 0;
}

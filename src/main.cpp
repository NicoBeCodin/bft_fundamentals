#include "../include/node.h"
#include "../include/bft_consensus.h"
#include "../include/thread_network.h"
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc < 2){
    std::cerr << "Usage: ./run [number_of_nodes]";
    throw std::runtime_error("Not enough arguments");
  }
  
  uint32_t n_nodes = std::stoi(argv[1]);

  auto shared_inboxes = std::make_shared<SharedInboxes>(n_nodes);
  std::vector<std::unique_ptr<Node>> nodes;
  nodes.reserve(n_nodes);

  //Launch nodes
  for (size_t i =0; i< n_nodes; ++i){
    auto consensus = std::make_unique<BFTConsensus>();
    auto thread_transport = std::make_unique<ThreadTransport>(i, shared_inboxes);
    nodes.push_back(std::make_unique<Node>(i, n_nodes, std::move(thread_transport), std::move(consensus)));
  }

  std::cout << "Starting " << n_nodes << " nodes\n";
  for (auto& n: nodes){
    n->start();
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  //Initial message

  nodes[0]->propose_random_block();


  

  
 
  
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }

  return 0;
}

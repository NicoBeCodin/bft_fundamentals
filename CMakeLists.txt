cmake_minimum_required(VERSION 3.16)
project(bft_node LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(bft_node
  src/main.cpp
  src/node.cpp
  src/tcp_transport.cpp
  src/bft_consensus.cpp
  src/message.cpp
)

target_include_directories(bft_node PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(bft_node PRIVATE Threads::Threads)

# Optional: nicer warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(bft_node PRIVATE -Wall -Wextra -Wpedantic)
endif()

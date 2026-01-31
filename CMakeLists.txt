cmake_minimum_required(VERSION 3.16)
project(bft_fundamentals LANGUAGES CXX)

find_package(OpenSSL REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(run
  src/main.cpp
  src/node.cpp
  src/bft_consensus.cpp
  src/message.cpp
  src/thread_network.cpp
)

target_include_directories(run PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(run PRIVATE Threads::Threads OpenSSL::Crypto)

# Optional: nicer warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(run PRIVATE -Wall -Wextra -Wpedantic)
endif()

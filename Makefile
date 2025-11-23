CXX = g++
CXXFLAGS  = -std=c++20 -Wall -Wextra -pedantic -O3

SRCS = bft.cpp network.cpp node.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = bft

.PHONY: all clean

all: $(TARGET)
	
clean:
	rm -f $(OBJS) $(TARGET)


$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
	

	

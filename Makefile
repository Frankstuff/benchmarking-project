



CXX = g++
CXXFLAGS = -Wall -Wextra -g -std=c++17
all: client server

client: myheader.h client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^
server: myheader.h server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^
clean:
	rm -f client

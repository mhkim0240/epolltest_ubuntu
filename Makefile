TARGET = simple_cpp_epoll_server

CFLAGS = --std=c++11
SOURCE = server.cpp
LDFLAGS = -pthread

all:
	g++ $(CFLAGS) -o $(TARGET) $(LDFLAGS) $(SOURCE)
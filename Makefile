CXX=g++
CXXOPTIMIZE= -O2
CXXFLAGS= -g -Wall -pthread -std=c++11 $(CXXOPTIMIZE)
USERID=EDIT_MAKE_FILE

# Add all .cpp files that need to be compiled for your server
SERVER_FILES=server.cpp

# Add all .cpp files that need to be compiled for your client
CLIENT_FILES=client.cpp

TEST_FILES=tcpTest.cpp

all: server client test

*.o: *.cpp
	$(CXX) -o $@ $^ $(CXXFLAGS) $@.cpp

server: $(SERVER_FILES:.cpp=.o)
	$(CXX) -o $@ $(CXXFLAGS) $(SERVER_FILES:.cpp=.o)

client: $(CLIENT_FILES:.cpp=.o)
	$(CXX) -o $@ $(CXXFLAGS) $(CLIENT_FILES:.cpp=.o)

test: $(TEST_FILES:.cpp=.o)
	$(CXX) -o $@ $(CXXFLAGS) $(TEST_FILES:.cpp=.o)

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client test *.tar.gz

tarball: clean
	tar -cvf $(USERID).tar.gz *

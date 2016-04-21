default: client_epoll server_epoll
client_epoll: client_epoll.cpp
	g++ client_epoll.cpp -o client_epoll
server_epoll: server_epoll.cpp
	g++ server_epoll.cpp -o server_epoll

clean:
	rm -f client_epoll server_epoll

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <map>
using namespace std;

#define MAX_EVENTS (1 << 16)

void setnonblocking(int fd) 
{
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

enum State 
{
	kStateConnecting = 0,
	kStateWriting
};

struct SocketState 
{
	uint32_t addr;
	State state;
	SocketState() : addr(0), state(kStateConnecting) {}
};

const char* ntoa(in_addr_t addr) // взятие адреса в формате XXX.XXX.XXX.XXX
{
	struct in_addr a;
	a.s_addr = htonl(addr);
	return inet_ntoa(a);
}

#define ADDR_START 77*256*256*256 + 88 * 256 *256 + 40*256// 77.88.54.0 - первый адрес

#define ADDR_FINISH 77*256*256*256 + 88 * 256 *256 + 56 *256// 77.88.56.00 - последний адрес

int main() 
{
	struct sockaddr_in addr_in;
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(80);
	int epollfd = epoll_create(1);
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];
	map<int, SocketState> socks;
	const char* HTTP_GET = "GET / HTTP/1.1\r\n\r\n";
	uint32_t addr = ADDR_START;
	int sock;
	while (addr < ADDR_FINISH || socks.size() != 0) 
	{
		while (addr < ADDR_FINISH && socks.size() < MAX_EVENTS) 
		{
			// register new socket
			sock = socket(AF_INET, SOCK_STREAM, 0);
			setnonblocking(sock);
			addr_in.sin_addr.s_addr = htonl(addr);
			if (connect(sock, (struct sockaddr*)(&addr_in), sizeof(addr_in))) 
			{
				if (errno != EINPROGRESS) 
				{
					printf("IP %s: connect\n", ntoa(addr));
					perror("");
					addr++;
					continue;
				}
			}
			ev.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
			ev.data.fd = sock;
			if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev)) 
			{
				perror("epoll_ctl: sock");
				exit(EXIT_FAILURE);
			}
			socks[sock].addr = addr;
			addr++;
		}
		int events_num = epoll_wait(epollfd, events, MAX_EVENTS, 7000);
		if (events_num == 0) 
		{
			for (auto sock : socks) 
			{
				struct in_addr addr;
				addr.s_addr = htonl(sock.second.addr);
				printf("IP %s: connection timed out\n", inet_ntoa(addr));
				close(sock.first);
			}
			socks.clear();
		}
    for (int i = 0; i < events_num; i++) 
		{
			sock = events[i].data.fd;
			SocketState& state = socks[sock];
			if (events[i].events & EPOLLERR) 
			{
				const char *sock_error = "unknown error";
				int error = 0;
				socklen_t errlen = sizeof(error);
				getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen);
				if (error) 
				{
					sock_error = strerror(error);
				}
				// error
				printf("IP %s: %s\n", ntoa(state.addr), sock_error);
				socks.erase(sock);
				close(sock);
			}
			else switch (state.state) 
			{
				case kStateConnecting: 
				{
		       // ready to write
					dprintf(sock, HTTP_GET, ntoa(state.addr));
					// change epoll type to EPOLLIN
					ev.data.fd = sock;
					ev.events = EPOLLIN | EPOLLET;
					if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sock, &ev)) 
					{
						perror("epoll_ctl: modify sock");
						exit(EXIT_FAILURE);
					}
					state.state = kStateWriting;
					break;
				}
				default:		//case kStateWriting: 
				{
					// ready to read server's response
					char buf[1024] = {0};
					int rsize;
					printf("IP %s: \n", ntoa(state.addr));
					while ((rsize = read(sock, buf, sizeof(buf)))) 
					{
						if (rsize < 0) 
						{
							perror("read");
							break;
						}
						write(STDOUT_FILENO, buf, rsize);
					}
					// unregister from epoll
					if (epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, NULL)) 
					{
						perror("epoll_ctl: modify sock");
						exit(EXIT_FAILURE);
					}
					socks.erase(sock);
					close(sock);
					break;
				}
			} // switch
		} // epoll events for
	} // while
	close(epollfd);
	return 0;
}

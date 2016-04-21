#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

int main() 
{
	int sock;
	struct sockaddr_in addr, local;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("socket\n");
		exit(EXIT_FAILURE);
	}
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5000);
	addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("connect\n");
		exit(EXIT_FAILURE);
	}
	int conn_sock, flags;
	int epollfd = epoll_create(2);
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sock;  
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev)) 
	{
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}
	char buf[128] = {0};	
	scanf("%s", buf);
	send(sock, buf, sizeof(buf), 0);	
	read(ev.data.fd, buf, sizeof(buf));
	printf("%s\n", buf);	
	close(epollfd);
	close(sock);
  return 0;
}

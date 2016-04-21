#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>

int main() 
{
	int listen_sock, events_num, conn_sock, flags;
	if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("sock\n");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in addr, local;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5000);
	addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	if (bind(listen_sock, reinterpret_cast <struct sockaddr*>(&addr), sizeof(addr))) 
	{
		perror("bind\n");
		exit(EXIT_FAILURE);
	}
	if (listen(listen_sock, 10))
	{
		perror("listen\n");
		exit(EXIT_FAILURE);
	}
	int epollfd = epoll_create(2);
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = listen_sock;  
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev)) 
	{
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}
	struct epoll_event events[10];
	while (1) 
	{
		events_num = epoll_wait(epollfd, events, 10, -1);
		for (int i = 0; i < events_num; i++) 
		{
			if (events[i].data.fd == listen_sock) 
			{
		     socklen_t addrlen;
				conn_sock = accept(listen_sock, (struct sockaddr *)&local, &addrlen);
				if (conn_sock == -1) 
				{
					perror("accept");
					exit(EXIT_FAILURE);
				}
				flags = fcntl(conn_sock, F_GETFL, 0);
				fcntl(conn_sock, F_SETFL, flags | O_NONBLOCK);
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = conn_sock;
				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev)) 
				{
					perror("epoll_ctl: conn_sock");
					exit(EXIT_FAILURE);
				}
			} 
			else 
			{
				char buf[256] = {0};		
				read(events[i].data.fd, buf, sizeof(buf));
				printf("%s\n", buf);
				scanf("%s", buf);
				send(events[i].data.fd, buf, sizeof(buf), 0);	
  		}
		}
	}
  close(epollfd);
	close(listen_sock);
  return 0;
}

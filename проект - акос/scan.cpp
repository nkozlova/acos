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

void setnonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

enum State{															// состояние сокета - connect or write
	kStateConnecting = 0,
	kStateWriting
};

struct SocketState {
	uint32_t addr;												// адрес
	State state;													// состояние - connect or write
	SocketState() : addr(0), state(kStateConnecting) {}
};

const char* in_ntoa(in_addr_t addr) {		// взятие адреса в формате XXX.XXX.XXX.XXX
	struct in_addr a;
	a.s_addr = htonl(addr);
	return inet_ntoa(a);
}

#define ADDR_START 0
// первый адрес - 0.0.0.0
#define ADDR_FINISH 4294967295
//последний адрес - 255.255.255.255 -- 255*256*256*256+255*256*256+255*256+255

#define MAX_EVENTS (1 << 16)
// максимальное кол-во

#define SIZE (3 * 17)
// длина сторки сканирования - 3 порта и делитель разности адресов
#define SIZE1 ((ADDR_FINISH - ADDR_START) / 17)
// промежуток событий между ## - кол-во новых закрытых socks

void print(int& k, long int l) { 				// вывод строки сканирования
	// когда закрываем очередной sock - выводим новое состояние строки
  if (l % (SIZE1) == 0) {
		printf("\r|");
		if (isatty(STDOUT_FILENO)) printf("\033[42m");
		for (int i = 0; i < SIZE; i++) printf(i <= k? "#" : " ");
		if (isatty(STDOUT_FILENO)) printf("\033[0m");	
		printf("|");
		fflush(stdout);
		k++;
		sleep(1);
	}	
} 

int get_sock_error(int sock) {
  int error = 0;
  socklen_t errlen = sizeof(error);
  getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen);
  return error;
}     

int main() {
	int k = 0;														// длина просканированной части сроки
	long int l = 0;												// кол-во пройденных адресов
 	print(k, l);
	struct sockaddr_in addr_in;
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(80);
	int epollfd = epoll_create(1);
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];
	map<int, SocketState> socks;
	const char* HTTP_GET = "GET / HTTP/1.1\r\n\r\n";
	uint32_t addr;
	int sock;
	FILE *f;
	f = fopen("out.txt", "w");
	for(int j = 0; j < 3; j++) {
	addr = ADDR_START;
	addr_in.sin_port = (j == 0) ? htons(80) : ((j == 1) ? htons(1080) : htons(8080));
		while (addr < ADDR_FINISH || socks.size() != 0) {
			// регистрируем новый сокет
			while (addr < ADDR_FINISH && socks.size() < MAX_EVENTS) {
				sock = socket(AF_INET, SOCK_STREAM, 0);
				setnonblocking(sock);
				addr_in.sin_addr.s_addr = htonl(addr);
				if (connect(sock, (struct sockaddr*)(&addr_in), sizeof(addr_in))) {
					if (errno != EINPROGRESS) {
						print(k, ++l);
						fprintf(f, "IP %s: connect error - bad file descriptor\n", in_ntoa(addr));
						addr++;
						continue;
					}
				}
				ev.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
				ev.data.fd = sock;
				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev)) {
					// здесь perror("epoll_ctl: sock");
					exit(EXIT_FAILURE);
				}
				socks[sock].addr = addr;
				addr++;
			}
			// ждем подключений
			int events_num = epoll_wait(epollfd, events, MAX_EVENTS, 1000);
 			// если их не произошло - освобождаем socks
			if (events_num == 0) {
				for (auto sock : socks) {
					struct in_addr addr;
					addr.s_addr = htonl(sock.second.addr);
					fprintf(f, "IP %s: connection timed out\n", inet_ntoa(addr));
					close(sock.first);
					print(k, ++l);
				}
				socks.clear();
			}
		  for (int i = 0; i < events_num; i++) {
				sock = events[i].data.fd;
				SocketState& state = socks[sock];
			 	// если ошибка - закрываем сокет
				if (events[i].events & EPOLLERR) {
					const char *sock_error = "unknown error";
					int error = 0;
					socklen_t errlen = sizeof(error);
					getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen);
					if (error) {
						sock_error = strerror(error);
					}					
					fprintf(f, "IP %s: %s\n", in_ntoa(state.addr), sock_error);
					socks.erase(sock);
					close(sock);
	 				print(k, ++l);
				}
				else switch (state.state) {
					case kStateConnecting:{ 				// мы в положении connect
						// считываем информацию
						dprintf(sock, HTTP_GET, in_ntoa(state.addr));
						// меняем тип epoll на EPOLLIN
						ev.data.fd = sock;
						ev.events = EPOLLIN | EPOLLET;
						if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sock, &ev)) {
							perror("epoll_ctl: modify sock");
							exit(EXIT_FAILURE);
						}
						// меняем сосотояние на write
						state.state = kStateWriting;
						break;
					}
					default:{												//мы в положении write
						char buf[1024] = {0};
						int rsize;
						fprintf(f, "IP %s: \n", in_ntoa(state.addr));
						bool finished = true;
						while ((rsize = read(sock, buf, sizeof(buf)))) {
							if (rsize < 0) {
								int error = get_sock_error(sock);
								if (error && error != EAGAIN) {
									perror("read");
								} else {
									finished = false;
								}
								break;
							}
						}
						fprintf(f, "%s", buf);
						if (finished) {
							// исключаем завершенный сокет из epoll
							if (epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, NULL)) {
								perror("epoll_ctl: modify sock");
								exit(EXIT_FAILURE);
							}
							socks.erase(sock);
							print(k, ++l);
							close(sock);
						}
						break;
					}
				} // switch
			} // epoll events for
		} // while
	}	// for
	close(epollfd);
	fclose(f);
	return 0;
}

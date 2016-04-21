#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <list>
using namespace std;

#define THREAD_N 20

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

list<int> queue;

void *entry(void *) 
{
	while (1) 
	{
		pthread_mutex_lock(&mutex);
		while (!queue.size()) 
		{
    	assert(!pthread_cond_wait(&cond, &mutex));
    }
    int sock = queue.front();
    queue.pop_front();
    pthread_mutex_unlock(&mutex);
    if (!sock) 
		{
    	break;
    }
		char buf[256] = {0};
    read(sock, buf, sizeof(buf));
    printf("%s\n", buf);
		close(sock);
	}
}


int main()
{
	int listener;
	struct sockaddr_in addr;
	char buf[256];
	int bytes_read;
	int sock;
	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener < 0) 
	{
		perror("sock\n");
		exit(1);
	}
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5000); 
	addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
	{
		perror("bind\n");
		exit(1);
	}
	if(listen(listener, 1) == -1) 
	{
		perror("listener\n");
		exit(1);
	}
	pthread_t *pool = new pthread_t[THREAD_N];
	for (int i = 0; i < THREAD_N; i++) 
	{
		pthread_create(pool + i, NULL, entry, NULL);
	}
	while (1) 
	{
		sock = accept(listener, NULL, NULL);
		if (sock < 0) 
		{
			perror("accept\n");
			exit(1);
		}
		pthread_mutex_lock(&mutex);
		queue.push_back(sock);
		pthread_mutex_unlock(&mutex);
		pthread_cond_signal(&cond);
		close(sock);
	}
	for (int i = 0; i < THREAD_N; i++) 
	{
		pthread_join(pool[i], NULL);
	}
	delete [] pool;
	return 0;
}

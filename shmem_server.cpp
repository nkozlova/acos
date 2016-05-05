#include <sys/errno.h>
#include <unistd.h>
#include <sys/shm.h> 
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/un.h>

#define SHMEM_PATH "/my_shmem"
#define SHMEM_SIZE (1 << 16)  // 64 KiB

int main() 
{
	int res, shmem_fd;
	void *ptr;
	shmem_fd = shm_open(SHMEM_PATH, O_RDWR | O_CREAT, 0666);
	if (shmem_fd < 0) 
	{
		perror("shm_open\n");
		exit(1);
	}
	res = ftruncate(shmem_fd, SHMEM_SIZE);
	if (res) 
	{
		perror("ftruncate\n");
		exit(1);
	}
	ptr = mmap(NULL, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmem_fd, 0);
	if (!ptr) 
	{
		perror("mmap\n");
		exit(1);
	}
	*(int*)ptr = 42;
	int sock, listener;
	struct sockaddr_un addr;
	listener = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listener < 0) 
	{
		perror("socket\n");
		exit(1);
	}
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SHMEM_PATH);
	if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
	{
		perror("bind\n");
		exit(1);	
	}
	if(listen(listener, 1) == -1) 
	{
		perror("listen\n");
		exit(1);
	}
	sock = accept(listener, NULL, NULL);
	if (sock == -1) 
	{
		perror("accept\n");
		exit(1);
	}
	char buf[256] = {0};
	recv(sock, buf, sizeof(buf), 0);
	printf("%s\n", buf);
	close(sock);
	res = close(shmem_fd);
  if (res) 
	{
   	perror("close\n");
   	exit(1);
  }
  res = shm_unlink(SHMEM_PATH);
	if (res) 
	{
	 	perror("shm_unlink\n");
	  exit(1);
	}
	return 0;
}

default:  shmem_server shmem_client

shmem_server: shmem_server.cpp
	g++ shmem_server.cpp -o shmem_server -lrt
shmem_client: shmem_client.cpp
	g++ shmem_client.cpp -o shmem_client -lrt

clean:
	rm -f shmem_client shmem_server

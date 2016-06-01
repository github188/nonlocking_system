
all:
	gcc  -I./  poll.c thread_pool.c socket_event.c main.c    -o  test_bin  -lpthread

clean:
	-rm -rf  test_bin
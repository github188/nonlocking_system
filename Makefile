
all:
	gcc  -I./   thread_pool.c  main.c    -o  test_bin  -lpthread

clean:
	-rm -rf  test_bin
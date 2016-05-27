#ifndef _socket_send_h
#define _socket_send_h


typedef struct send_socket_node
{
	int socket_fd;
	int length;
	int need_free;
	void * data;
}send_socket_node_t;




int socket_send_fun(void * ud);


#endif/*_socket_send_h*/
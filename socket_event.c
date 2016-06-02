#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h> 
#include <sys/select.h> 
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdint.h>



#include "poll.h"
#include "common.h"




#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"socket_event:"


#define  EVENT_FD_NUM	(1024)

typedef  enum type_cmd
{	
	CMD_BEGIN,
	SOCKET_OPEN,
	SOCKET_CLOSE,
	SOCKET_LISTEN,
	SOCKET_START,
	SOCKET_BIND,
	CMD_END,
	
}type_cmd_m;






typedef enum type_status
{
	TYPE_INVALID,
	TYPE_LISTEN,
	TYPE_CONNECTING,
	TYPE_CONNECTED,
	TYPE_PACCEPT,
	TYPE_BIND,
}type_status_m;



typedef struct data_buffer
{
	struct data_buffer * next;
	void * data;
	unsigned int length;
	void * node;   /*套接字节点*/

}data_buffer_t;


/*发送链表*/
typedef  struct wb_list
{
	data_buffer_t * head;
	data_buffer_t * tail;
	
}wb_list_t;


/*接收链表*/
typedef  struct rd_list
{
	data_buffer_t * head;
	data_buffer_t * tail;
	
}rd_list_t;




typedef  struct event_node
{
	int fd;
	int status;
	unsigned int id;
	void (*pfun)(void * arg);
	void * ud;
	uintptr_t handle; 
}event_node_t;




typedef struct socket_event
{
	int poll_fd;
	int dist_pipe_read;
	int dist_pipe_write;
	int cmd_pipe_read;
	int cmd_pipe_write;
	int socket_send_pipe_read;
	int socket_send_pipe_write;
	event_node_t node[EVENT_FD_NUM];
	wb_list_t wblist;
	rd_list_t rdlist;
	
}socket_event_t;











typedef  struct cmd_node
{
	type_cmd_m cmd;
	int fd;
	unsigned int id;
	
}cmd_node_t;


union sockaddr_all 
{
	struct sockaddr s;
	struct sockaddr_in v4;
	struct sockaddr_in6 v6;
};




static socket_event_t * socket_handle = NULL;





static int event_id_enqueue(socket_event_t * handle,unsigned int id)
{

	if(NULL==handle || handle->dist_pipe_write<0)
	{
		dbg_printf("check the param!\n");
		return(-1);
	}
	int socket_id = id;
	int ret = -1;
	ret = write(handle->dist_pipe_write,&socket_id,sizeof(socket_id));
	if(ret <= 0)
	{
		dbg_printf("write dist pipe is fail!\n");
		return(-1);
	}
	return(0);
}



static  int event_id_dequeue(socket_event_t * handle)
{

	int ret = -1;
	if(NULL==handle || handle->dist_pipe_read < 0)
	{
		dbg_printf("check the param!\n");
		return(-1);
	}
	struct timeval tv = {0,0};
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(handle->dist_pipe_read, &fds);
	ret = select(handle->dist_pipe_read+1, &fds, NULL, NULL, &tv);

	if(ret <= 0)return(-1);

	int id = -1;
	ret = read(handle->dist_pipe_read,&id,sizeof(id));
	return(id);
	
}



static  int event_write_cmd(type_cmd_m cmd,int fd,unsigned int id)
{

	socket_event_t * handle = socket_handle;
	if(NULL==handle || handle->cmd_pipe_write<=0)
	{
		dbg_printf("the handle has not been inited!\n");
		return(-1);

	}
	if(CMD_BEGIN>cmd || CMD_END<cmd)
	{
		dbg_printf("please check the param!\n");
		return(-1);
	}

	int ret = -1;
	cmd_node_t pipe_cmd;
	pipe_cmd.cmd = cmd;
	pipe_cmd.fd = fd;
	pipe_cmd.id = id;

	ret =  write(handle->cmd_pipe_write,&pipe_cmd,sizeof(cmd_node_t));
	if(ret <= 0)
	{
		dbg_printf("cmd_pipe_write is fail!\n");
		return(-1);
	}

	return(0);
}




static int event_read_cmd(cmd_node_t * node)
{
	socket_event_t * handle = socket_handle;
	if(NULL==handle || handle->cmd_pipe_read<=0)
	{
		dbg_printf("the handle has not been inited!\n");
		return(-1);
	}	

	if(NULL == node)
	{
		dbg_printf("the param is null!\n");
		return(-1);
	}

	int ret = read(handle->cmd_pipe_read,node,sizeof(cmd_node_t));
	return(ret);
}


static void socket_keepalive(int fd) 
{
	int keepalive = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive));
}




static int socket_do_bind(const char * host,int port,int protocol,int * family)
{

	if(IPPROTO_UDP !=protocol && IPPROTO_TCP != protocol)
	{
		dbg_printf("the protocol is not ok!\n");
		return(-1);
	}

	int ret = -1;
	int fd = -1;
	int status = 0;
	struct addrinfo ai_hints;
	struct addrinfo *ai_list = NULL;
	char portstr[16]={'\0'};
	
	if(NULL==host || 0==host[0])
	{
		host = "0.0.0.0";
	}
	snprintf(portstr,16,"%d",port);
	memset(&ai_hints,0,sizeof(ai_hints));
	ai_hints.ai_family = AF_UNSPEC;
	ai_hints.ai_protocol = protocol;
	
	if(IPPROTO_TCP==protocol)
	{
		ai_hints.ai_socktype = SOCK_STREAM;	
	}
	else
	{
		ai_hints.ai_socktype = SOCK_DGRAM;
	}
	
	status = getaddrinfo(host, portstr, &ai_hints, &ai_list);
	if (status != 0 )
	{
		dbg_printf("getaddrinfo is fail!\n");
		return (-1);
	}

	*family = ai_list->ai_family;
	fd = socket(*family,ai_hints.ai_socktype,0);
	if(fd < 0)
	{
		dbg_printf("socket is fail!\n");
		goto fail;
	}

	int reuse = 1;
	ret = setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(void*)&reuse,sizeof(int));
	if(ret < 0)
	{
		dbg_printf("setsockopt is fail!\n");
		goto fail;
	}

	ret = bind(fd,(struct sockaddr *)ai_list->ai_addr,ai_list->ai_addrlen);
	if(ret < 0)
	{
		dbg_printf("bind is fail!\n");
		goto fail;
	}
	
	freeaddrinfo(ai_list);
	return(fd);

fail:

	if(NULL != ai_list)
	{
		freeaddrinfo(ai_list);
		ai_list = NULL;
	}
	return(-1);

}



static int  socket_do_listen(const char * host,int port,int backlog)
{
	int family = 0;
	int listen_fd = -1;
	int ret = -1;
	listen_fd = socket_do_bind(host,port,IPPROTO_TCP,&family);
	if(listen_fd < 0)
	{
		dbg_printf("socket_do_bind is fail!\n");
		return(-1);
	}
	
	ret = listen(listen_fd,backlog);
	if(ret < 0)
	{
		dbg_printf("listen is fail!\n");
		close(listen_fd);
		listen_fd = -1;
		return(-1);
	}

	ret = event_write_cmd(SOCKET_LISTEN,listen_fd,0);
	if(ret < 0)
	{
		dbg_printf("event_write_cmd is fail!\n");
		close(listen_fd);
		listen_fd = -1;

		return(-1);
	}
	
	return(0);
}




static void  socket_listen_server(void * arg)
{
	event_node_t * node = (event_node_t*)arg;
	if(NULL == node)
	{
		dbg_printf("please check the param!\n");
		return;
	}
	socket_event_t * handle = (socket_event_t*)node->handle;
	if(NULL==handle)
	{
		dbg_printf("handle is not been inited!\n");
		return;
	}
	
	int client_fd = -1;
	union sockaddr_all u;
	socklen_t len = sizeof(u);

	client_fd = accept(node->fd,&u.s,&len);
	if(client_fd < 0)
	{
		dbg_printf("accept is wrong!\n");
		return;
	}

	int id = event_id_dequeue(handle);
	if(client_fd < 0 )
	{
		close(client_fd);
		client_fd = -1;
	}

	event_node_t * client_node = (event_node_t*)&handle->node[id];
	
	client_node->fd = client_fd;
	client_node->id = id;
	client_node->pfun = NULL;
	client_node->ud = NULL;
	socket_keepalive(client_fd);
	poll_nonblocking(client_fd);

	int ret = -1;
	ret = poll_add(handle->poll_fd,client_node->fd,client_node);
	if(0 != ret)
	{
		event_id_enqueue(handle,id);
		close(client_node->fd);
		client_node->fd = -1;
		return;
	}
	
	dbg_printf("the client id==%d is accept ok!\n",id);

}





static void * event_poll(void * arg)
{
	socket_event_t * handle = (socket_event_t*)arg;
	if(handle->cmd_pipe_read<=0 || handle->dist_pipe_read<=0 || handle->poll_fd<=0)
	{
		dbg_printf("please check the param!\n");
		return(NULL);
	}

	event_node_t * node = NULL;
	event_t poll_events[1024];
	int n = -1;
	int is_run = 1;
	int i = 0;
	
	while(is_run)
	{
		n = poll_wait(handle->poll_fd,poll_events,1024);
		if(n <= 0)continue;

		for(i=0;i<n;++i)
		{
			node = 	(event_node_t*)poll_events[i].ud;
			dbg_printf("the server id is %d\n",node->id);
			if(NULL != node && NULL != node->pfun)
			{
				node->pfun(node->ud);	
			}
		}



	}


	return(NULL);
}





int socket_evnet_init(void)
{
	if(NULL != socket_handle)
	{
		dbg_printf("the socket handle has been inited!\n");
		return(-1);
	}

	socket_event_t * handle = calloc(1,sizeof(*handle));
	if(NULL == handle)
	{
		dbg_printf("calloc is fail!\n");
		return(-1);
	}


	int ret = -1;
	int i = 0;
	int pipe_fd[2] = {-1,-1};
	ret = pipe(pipe_fd);
	if(ret < 0 )
	{
		dbg_printf("pipe is fail!\n");
		goto fail;
	}

	handle->dist_pipe_read  = pipe_fd[0];
	handle->dist_pipe_write = pipe_fd[1];
	poll_nonblocking(handle->dist_pipe_read);
	poll_nonblocking(handle->dist_pipe_write);

	

	ret = pipe(pipe_fd);
	if(ret < 0 )
	{
		dbg_printf("pipe is fail!\n");
		goto fail;
	}

	handle->cmd_pipe_read  = pipe_fd[0];
	handle->cmd_pipe_write = pipe_fd[1];

	poll_nonblocking(handle->cmd_pipe_read);
	poll_nonblocking(handle->cmd_pipe_write);


	handle->poll_fd = poll_new();
	if(handle->poll_fd < 0)
	{
		dbg_printf("poll_new is fail!\n");
		goto fail;
	}

	handle->rdlist.head = NULL;
	handle->rdlist.tail = NULL;
	handle->wblist.head = NULL;
	handle->wblist.tail = NULL;
	
	socket_handle = handle;

	return(0);

fail:


	if(handle->dist_pipe_read > 0)
	{
		close(handle->dist_pipe_read);
		handle->dist_pipe_read = -1;
	}

	if(handle->dist_pipe_write > 0)
	{
		close(handle->dist_pipe_write);
		handle->dist_pipe_write = -1;
	}

	if(handle->cmd_pipe_read>0)
	{
		close(handle->cmd_pipe_read);
		handle->cmd_pipe_read = -1;
	}

	if(handle->cmd_pipe_write > 0 )
	{
		close(handle->cmd_pipe_write);
		handle->cmd_pipe_write = -1;

	}
	
	if(handle->poll_fd > 0)
	{
		poll_close(handle->poll_fd);
	}
	

	if(NULL != handle)
	{
		free(handle);
		handle = NULL;

	}
	return(-1);

}




static void  cmd_pipe_server(void * arg)
{

	int ret  = -1;
	cmd_node_t node_read;
	socket_event_t * handle = (socket_event_t*)arg;
	if(NULL == handle)
	{
		dbg_printf("the handle has not been initd!\n");
		return;

	}

	ret = event_read_cmd(&node_read);
	if(ret<=0)
	{
		dbg_printf("cmd_pipe_server read is fail!\n");
		return;
	}
	switch(node_read.cmd)
	{
		case SOCKET_LISTEN:
		{
			if(0 != node_read.id || node_read.fd <= 0)
			{
				dbg_printf("the id have alloced !\n");
				break;
			}
			int id = event_id_dequeue(handle);
			if(id < 0)
			{
				dbg_printf("calloc id is fail!\n");
				close(node_read.fd);
				node_read.fd = -1;
				break;
			}

			int ret = -1;
			event_node_t * node = &handle->node[id];
			node->fd = node_read.fd;
			node->id = id;
			node->pfun = socket_listen_server;
			node->ud = node;
			ret = poll_add(handle->poll_fd,node->fd,node);
			if(0 != ret)
			{
				event_id_enqueue(handle,id);
				close(node_read.fd);
				node_read.fd = -1;
				break;
			}
			dbg_printf("create listen id is ok!\n");

			break;
		}

		default:
		{
			dbg_printf("unknow cmd!\n");
			break;
		}




	}

	dbg_printf("send cmd's id===%d\n",node_read.id);
	
}



int socket_event_startup(void)
{
	int ret = -1;

	ret = socket_evnet_init();
	if(ret < 0 )
	{
		dbg_printf("socket_evnet_init is fail!\n");
		return(-1);
	}

	socket_event_t * handle = socket_handle;
	int i = 0;

	for(i=0;i<EVENT_FD_NUM; ++i)
	{
		event_node_t * node = &handle->node[i];
		node->id = i;
		node->fd = -1;
		node->pfun = NULL;
		node->ud = NULL;
		node->handle = (uintptr_t)handle;
		event_id_enqueue(handle,node->id);
	}

	int id = event_id_dequeue(handle);
	if(id < 0)
	{
		dbg_printf("please inqueue the id first!\n");
		return(-1);
	}

	event_node_t * node_pipe = &handle->node[id];
	node_pipe->fd = handle->cmd_pipe_read;
	node_pipe->id = id;
	node_pipe->ud = handle;
	node_pipe->pfun = cmd_pipe_server;
	
	ret = poll_add(handle->poll_fd,handle->cmd_pipe_read,node_pipe);
	if(0 != ret)
	{
		dbg_printf("poll_add is fail!\n");
		return(-1);
	}

	pthread_t poll_ptd;
	pthread_create(&(poll_ptd), NULL, event_poll, (void *)handle);
	pthread_detach(poll_ptd);

	sleep(3);
	while(1)
	{
		socket_do_listen("127.0.0.1",8888,32);
		sleep(2);
	}


	return(0);
}


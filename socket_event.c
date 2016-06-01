#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h> 
#include <sys/select.h> 
#include <pthread.h>

#include "poll.h"


#include "common.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"socket_event:"


#define  EVENT_FD_NUM	(1024)



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
	unsigned int id;
	void (*pfun)(void * arg);
	void * ud;
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






typedef  struct cmd_node
{
	type_cmd_m cmd;
	int fd;
	unsigned int id;
	
}cmd_node_t;




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


int event_add_watch()
{



	return(0);	
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
	ret = event_read_cmd(&node_read);
	if(ret<=0)
	{
		dbg_printf("cmd_pipe_server read is fail!\n");
		return;
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
	node_pipe->ud = NULL;
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
		event_write_cmd(SOCKET_OPEN,100,100);
		sleep(1);
	}


	return(0);
}


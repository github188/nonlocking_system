#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "poll.h"


#include "common.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"socket_event:"


#define  SOCKET_MAX_NUM		(4096)


typedef struct write_buffer
{
	struct write_buffer * next;
	void * data;
	unsigned int length;

}write_buffer_t;



typedef  struct wb_list
{
	write_buffer_t * head;
	write_buffer_t * tail;
	
}wb_list_t;



typedef  struct socket_node
{
	int fd;
	unsigned int id;
	wb_list_t list;
	
}socket_node_t;




typedef struct socket_event
{
	int poll_fd;
	int dist_pipe_read;
	int dist_pipe_write;
	int cmd_pipe_read;
	int cmd_pipe_write;
	int socket_send_pipe_read;
	int socket_send_pipe_write;
	socket_node_t node[SOCKET_MAX_NUM];
	
}socket_event_t;




static socket_event_t * socket_handle = NULL;





static int socket_enqueue(unsigned int id)
{
	socket_event_t * handle = socket_handle;
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



static unsigned int socket_dequeue(void)
{
	int ret = -1;
	socket_event_t * handle = socket_handle;
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

	unsigned int socket_id = 0;
	ret = read(handle->dist_pipe_read,&socket_id,sizeof(socket_id));
	
	return(socket_id);
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


	ret = pipe(pipe_fd);
	if(ret < 0 )
	{
		dbg_printf("pipe is fail!\n");
		goto fail;
	}

	handle->cmd_pipe_read  = pipe_fd[0];
	handle->cmd_pipe_write = pipe_fd[1];
	

	handle->poll_fd = poll_new();
	if(handle->poll_fd < 0)
	{
		dbg_printf("poll_new is fail!\n");
		goto fail;
	}


	for(i=0;i<SOCKET_MAX_NUM; ++i)
	{
		socket_node_t * node = &handle->node[i];
		node->id = i;
		node->fd = -1;
		node->list.head = NULL;
		node->list.tail = NULL;
	}


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
	for(i=0;i<SOCKET_MAX_NUM; ++i)
	{
		socket_node_t * node = &handle->node[i];
		socket_enqueue(node->id);
	}

	ret = poll_add(handle->poll_fd,handle->cmd_pipe_read,NULL);
	if(0 != ret)
	{
		dbg_printf("poll_add is fail!\n");
		return(-1);
	}

	return(0);
}


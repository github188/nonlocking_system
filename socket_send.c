#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"
#include "socket_send.h"



#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"socket_send:"



typedef struct socket_send
{
	int pipe_back_write;
	int pipe_back_read;
	
}socket_send_t;



static socket_send_t * socket_send_handle = NULL;



static int socketsend_init(void)
{
	if(NULL !=  socket_send_handle)
	{
		dbg_printf("the handle has been init !\n");
		return(-1);
	}

	socket_send_t * handle = calloc(1,sizeof(*handle));
	if(NULL==handle)
	{
		dbg_printf("calloc is fail!\n");
		return(-1);
	}


	int ret = -1;
	int fd[2] = {-1,-1};
	ret = pipe(fd);
	if(ret < 0 )
	{
		dbg_printf("pipe is fail!\n");
		goto fail;
	}
	handle->pipe_back_read = fd[0];
	handle->pipe_back_write = fd[1];

	socket_send_handle = handle;
	return(0);


fail:


	if(handle->pipe_back_read > 0)
	{
		close(handle->pipe_back_read);
		handle->pipe_back_read = -1;
	}


	if(handle->pipe_back_write)
	{
		close(handle->pipe_back_write);
		handle->pipe_back_write = -1;
	}


	if(NULL != handle)
	{
		free(handle);
		handle = NULL;
	}

	return(-1);
}




int socketsend_get_readpipe(void)
{
	socket_send_t * handle = socket_send_handle;
	if(NULL == handle)return(-1);
	return(handle->pipe_back_read);
}


int socketsend_get_writepipe(void)
{
	socket_send_t * handle = socket_send_handle;
	if(NULL == handle)return(-1);
	return(handle->pipe_back_write);
}


int socketsend_write_backpipe(int fd,void * data,int length)
{
	int ret = -1;
	socket_send_t * handle = socket_send_handle;
	if(NULL == handle)return(-1);
	

	if(NULL==data || length<=0 || fd <0)
	{
		dbg_printf("please check the param!\n");
		return(-1);
	}
	send_socket_node_t  node ;
	node.socket_fd = fd;
	node.data = data;
	node.length = length;
	node.need_free = 1;
	return(write(handle->pipe_back_write,&node,sizeof(node)));

}


/*只有当线程池中的线程可以的时候才去读取back pipe中的数据*/
int socketsend_read_backpipe(send_socket_node_t * node)
{

	socket_send_t * handle = socket_send_handle;
	if(NULL == handle)return(NULL);

	int ret = -1;
	struct timeval tv = {0,0};
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(handle->pipe_back_read, &fds);
	ret = select(handle->pipe_back_read+1, &fds, NULL, NULL, &tv);
	if(ret <= 0)return(-1);

	ret = read(handle->pipe_back_read,node,sizeof(send_socket_node_t));
	if(ret != sizeof(send_socket_node_t))
	{
		free(node);
		node = NULL;
	}
	return(0);	
}



int socketsend_fun(void * ud)
{

	send_socket_node_t * send_node = (send_socket_node_t*)ud;
	int n = -1;
	if(NULL !=send_node && NULL !=send_node->data && send_node->length>0)
	{
		n = write(send_node->socket_fd,send_node->data,send_node->length);
	}
	if(send_node->need_free && NULL != send_node)
	{
		if(NULL !=send_node->data)
		{
			free(send_node->data);
			send_node->data=NULL;
		}

		free(send_node);
		send_node = NULL;
	}

	return(n);
}

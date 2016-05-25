#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include "common.h"
#include "thread_pool.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"thread_pool:"



#define USER_DATA_MAX_LENGTH 	(4096)


typedef struct pthread_node
{
	pthread_t pthread_id;
	int pipe_read;
	int pipe_write;
	void (*fun)(void * arg);
	unsigned int id;
	int  user_data_length;
	char user_data_contex[USER_DATA_MAX_LENGTH];
	
}pthread_node_t;



typedef struct thread_pool
{
	int dist_pipe_write;
	int dist_pipe_read;
	int back_pipe_write;
	int back_pipe_read;
	pthread_node_t node[THREAD_POOL_MAX];

}thread_pool_t;




static thread_pool_t * thread_pool_handle = NULL;


static int pthread_enqueue(unsigned int id)
{
	thread_pool_t * pool = 	thread_pool_handle;
	if(NULL==pool || pool->dist_pipe_write<0)
	{
		dbg_printf("check the param!\n");
		return(-1);
	}
	int pthread_id = id;
	int ret = -1;
	ret = write(pool->dist_pipe_write,&pthread_id,sizeof(pthread_id));
	if(ret <= 0)
	{
		dbg_printf("write dist pipe is fail!\n");
		return(-1);
	}
	return(0);
}



static unsigned int pthread_dequeue(void)
{
	int ret = -1;
	thread_pool_t * pool = 	thread_pool_handle;
	if(NULL==pool || pool->dist_pipe_read < 0)
	{
		dbg_printf("check the param!\n");
		return(-1);
	}
	struct timeval tv = {0,0};
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(pool->dist_pipe_read, &fds);
	ret = select(pool->dist_pipe_read+1, &fds, NULL, NULL, &tv);
	if(ret <= 0)return(-1);

	unsigned int pthread_id = 0;
	ret = read(pool->dist_pipe_read,&pthread_id,sizeof(pthread_id));
	
	return(pthread_id);
}



static void* thread_pool_fun(void * arg)
{

	pthread_node_t * node = (pthread_node_t *)arg;
	if(NULL == node)
	{
		dbg_printf("please check the param!\n");
		pthread_exit(NULL);

	}

	unsigned int pipe_data;
	int ret = -1;
	int is_run = 1;
	while(is_run)
	{
		ret = read(node->pipe_read,&pipe_data,sizeof(pipe_data));
		if(ret>0 && NULL != node->fun)
		{
			node->fun(node->user_data_contex);	
			node->fun = NULL;
		}
		dbg_printf("the server id is %d\n",node->id);
		pthread_enqueue(node->id);
	}

	return(NULL);
}



static int thread_pool_init(void)
{
	int ret = -1;
	int pipe_fd[2] = {-1,-1};
	if(NULL != thread_pool_handle)
	{
		dbg_printf("thread_pool_handle has been inited!\n");
		return(-1);
	}
	
	thread_pool_t * pool = calloc(1,sizeof(*pool));
	if(NULL == pool)
	{
		dbg_printf("calloc is fail!\n");
		return(-1);
	}

	
	
	ret = pipe(pipe_fd);
	if(ret < 0 )
	{
		dbg_printf("pipe is fail!\n");
		goto fail;
	}

	pool->dist_pipe_read  = pipe_fd[0];
	pool->dist_pipe_write = pipe_fd[1];


#if 0
	ret = pipe(pipe_fd);
	if(ret < 0 )
	{
		dbg_printf("pipe is fail!\n");
		goto fail;
	}

	pool->back_pipe_read  = pipe_fd[0];
	pool->back_pipe_write = pipe_fd[1];
#endif

	int i = 0;
	for(i=0;i<THREAD_POOL_MAX;++i)
	{
		pthread_node_t * node = &pool->node[i];
		int fd[2] = {-1,-1};
		if(pipe(fd)<0)
		{
			dbg_printf("pipe is fail! i==%d errno==%d\n",i,errno);
			perror("error:");
			goto fail;
		}
		node->pipe_read = fd[0];
		node->pipe_write = fd[1];
		node->fun = NULL;
		node->id = i;
		pthread_create(&(node->pthread_id), NULL, thread_pool_fun, (void *)node);
		pthread_detach(node->pthread_id);
	}
	
	thread_pool_handle = pool;
	
	return(0);


fail:

	if(pool->back_pipe_read > 0 )
	{
		close(pool->back_pipe_read);
		pool->back_pipe_read = -1;
	}

	if(pool->back_pipe_write > 0 )
	{
		close(pool->back_pipe_write);
		pool->back_pipe_write = -1;
	}

	if(pool->dist_pipe_read > 0 )
	{
		close(pool->dist_pipe_read);
		pool->dist_pipe_read = -1;
	}

	if(pool->dist_pipe_write > 0 )
	{
		close(pool->dist_pipe_write);
		pool->dist_pipe_write = -1;
	}


	if(NULL != pool)
	{
		free(pool);
		pool = NULL;

	}

	return(-1);
}







int thread_add_job(void (*pfun)(void * arg),void * ud,int ud_lenght)
{
	unsigned int pthread_id = pthread_dequeue();
	if(pthread_id <= 0 || pthread_id>THREAD_POOL_MAX-1)
	{
		dbg_printf("fetch pthread is fail\n");
		return(-1);
	}
	
	if(ud_lenght > USER_DATA_MAX_LENGTH)
	{
		dbg_printf("the ud data length is out of the limit!\n");
		pthread_enqueue(pthread_id);

	}
	pthread_node_t * node =  &thread_pool_handle->node[pthread_id];
	node->fun = pfun;
	node->user_data_length = ud_lenght;
	memmove(node->user_data_contex,ud,ud_lenght);
	write(node->pipe_write,&node->id,sizeof(node->id));
	return(0);

}



int thread_pool_startup(void)
{
	int ret = -1;
	ret = thread_pool_init();
	if(ret < 0)
	{
		dbg_printf("thread_pool_init is fail!\n");
		return(-1);
	}

	int i = 0;
	for(i=0;i<THREAD_POOL_MAX; ++i)
	{
		pthread_node_t * node = &thread_pool_handle->node[i];
		pthread_enqueue(node->id);
	}
	return(0);
}







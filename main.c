#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "thread_pool.h"
#include "socket_event.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"main:"



/*ulimit   -n  4096*/


typedef struct test_node
{
	int a;
	int b;
	int c;
}test_node_t;


void test_pool(void * ud)
{
	test_node_t * node = (test_node_t*)ud;
	if(NULL == node)
	{
		dbg_printf("the param is null!\n");
		return;
	}
	dbg_printf("c=======%d\n",node->c);
	sleep(1);
	



}

int main(int argc,char ** argv)
{
	dbg_printf("this is a test for the main!\n");
	int ret = -1;
	ret = thread_pool_startup();
	if(ret < 0 )
	{
		dbg_printf("thread_pool_startup is fail!\n");
	}

	ret = socket_event_startup();
	if(0 != ret)
	{
		dbg_printf("socket_event_startup is fail!\n");
		return(-1);
	}

	
	test_node_t node;
	unsigned int counts = 0;
	while(1)
	{

		node.c = counts ++;
		thread_add_job(test_pool,&node,sizeof(node));
		usleep(1000);
	}

	while(1)sleep(100);
	return(0);
}


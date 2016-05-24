#ifndef _thread_pool_h
#define _thread_pool_h


#define THREAD_POOL_MAX		(1024)



int thread_pool_startup(void);
int thread_add_job(void (*pfun)(void * arg),void * ud,int ud_lenght);



#endif  /*_thread_pool_h*/
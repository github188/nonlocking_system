#ifndef _epoll_h_
#define _epoll_h_


typedef struct event
{
	void * ud;
	char read;
	char write;
}event_t;



int poll_new(void);
void poll_close(int efd);
int poll_add(int efd,int sock, void * ud);
void poll_del(int efd,int sock);
void  poll_modify(int efd,int sock,void * ud,char enbale);
int poll_wait(int efd,event_t * e,int max);
void poll_nonblocking(int fd);




#endif   /*_epoll_h_*/


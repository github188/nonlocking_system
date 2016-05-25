#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "common.h"
#include "epoll.h"


#undef  	DBG_ON
#undef  	FILE_NAME
#define 	DBG_ON  	(0x01)
#define 	FILE_NAME 	"epoll:"


#ifdef __linux__

#include <sys/epoll.h>

int poll_new(void)
{
	return(epoll_create(1024));
}



void poll_close(int efd)
{
	if(efd > 0)
	{
		close(efd);
		efd = -1;
	}

}



int poll_add(int efd,int sock, void * ud)
{

	int ret = -1;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = ud;

	ret = epoll_ctl(efd,EPOLL_CTL_ADD,sock,&ev);
	if(ret < 0 )
	{
		dbg_printf("epoll_ctl is fail!\n");
		return(-1);
	}
	return(0);
}




void poll_del(int efd,int sock)
{

	int ret = -1;
	if(efd<0 || sock < 0 )
	{
		dbg_printf("please check the param!\n");
		return;
	}
	
	ret = epoll_ctl(efd,EPOLL_CTL_DEL,sock,NULL);
	if(ret < 0)
	{
		dbg_printf("epoll_ctl is fail!\n");
	}

}



void  poll_modify(int efd,int sock,void * ud,char enbale)
{
	int ret = -1;
	struct epoll_event ev;
	ev.events = EPOLLIN | (enbale ? EPOLLOUT:0);
	ev.data.ptr = ud;
	ret = epoll_ctl(efd,EPOLL_CTL_MOD,sock,&ev);
	if(ret < 0)
	{
		dbg_printf("epoll_ctl is fail!\n");
	}

}





int poll_wait(int efd,event_t * e,int max)
{
	if(efd<0 || NULL==e || max <=0)
	{
		dbg_printf("please check the param!\n");
		return(-1);
	}
	struct epoll_event ev[max];
	int n = epoll_wait(efd,ev,max,-1);
	if(n <= 0)
	{
		dbg_printf("epoll wait is fail!\n");
		return(-1);

	}

	int i = 0;
	for(i=0;i<n;++i)
	{
		e[i].ud = ev[i].data.ptr;
		unsigned flag = ev[i].events;
		e[i].write = (flag&EPOLLOUT) != 0;
		e[i].read = (flag & EPOLLIN) != 0;
	}
	return(n);
	
}



void poll_nonblocking(int fd)
{
	if(fd < 0)
	{
		dbg_printf("the fd is invaild!\n");
		return;

	}
	int flag = fcntl(fd,F_GETFL,0);
	if(-1 != flag)
	{
		fcntl(fd,F_SETFL,flag|O_NONBLOCK);
	}
}



#else



int poll_new(void)
{
	return kqueue();
}


void poll_close(int kfd) 
{
	if(kfd>0)
	{
		close(kfd);
		kfd = -1;
	}
}



void poll_del(int kfd, int sock) 
{
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(kfd, &ke, 1, NULL, 0, NULL);
	EV_SET(&ke, sock, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(kfd, &ke, 1, NULL, 0, NULL);
}



int poll_add(int kfd, int sock, void *ud) 
{
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_READ, EV_ADD, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1) 
	{
		return 1;
	}
	EV_SET(&ke, sock, EVFILT_WRITE, EV_ADD, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1)
	{
		EV_SET(&ke, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		kevent(kfd, &ke, 1, NULL, 0, NULL);
		return 1;
	}
	EV_SET(&ke, sock, EVFILT_WRITE, EV_DISABLE, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1) 
	{
		sp_del(kfd, sock);
		return 1;
	}
	return 0;
}

void  poll_modify(int kfd, int sock, void *ud, char enable)
{
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_WRITE, enable ? EV_ENABLE : EV_DISABLE, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1)
	{

	}
}


int  poll_wait(int kfd, struct event *e, int max)
{
	struct kevent ev[max];
	int n = kevent(kfd, NULL, 0, ev, max, NULL);

	int i;
	for (i=0;i<n;i++)
	{
		e[i].s = ev[i].udata;
		unsigned filter = ev[i].filter;
		e[i].write = (filter == EVFILT_WRITE);
		e[i].read = (filter == EVFILT_READ);
	}

	return n;
}


void poll_nonblocking(int fd)
{
	int flag = fcntl(fd, F_GETFL, 0);
	if (-1 == flag )
	{
		return;
	}
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}


#endif

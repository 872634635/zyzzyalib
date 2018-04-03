//this is a service .cpp

#include<iostream>
using namespace std;
#include<string.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/select.h>
#include<fcntl.h>
#include<errno.h>
#include<vector>
#include<fstream>
#include<stdlib.h>
#include<sys/wait.h>
#include<pthread.h>
#define SERV_PORT 22223

void handleMsg(int sock);

int main()
{
	struct sockaddr_in servaddr;
	memset(&servaddr, '\0',sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	int listensock = socket(AF_INET,SOCK_STREAM,0);
	bind(listensock,(sockaddr *)&servaddr, sizeof(servaddr));
	listen(listensock,64);
	
	pid_t childpd = 0;
    int customersock = 0;
	while(true)
	{

		if(0 > (customersock = accept(listensock,NULL,NULL)))
		{	
			cout<<"error"<<endl;
			continue;	 
		}
		break;
	}
	if(childpd = fork() == 0)// return 0 means the child process
	{
		handleMsg(customersock);
		close(listensock);
		exit(0);
	}
	else
	{
		close(customersock);
		cout<<"this is a parent process"<<endl;
		int status = 0;
		int ret = wait(&status);
	}
	
	return 0;
}

void* thread_run(void * arg)
{
	// should use thread or process, otherwise, the system function will stop the child process result in can not receive the msg from customer
	cout<<"run"<<endl;
	system("./run.sh");
	return NULL;
}

void* thread_kill(void *arg)
{
	cout<<"kill"<<endl;
	system("./kill.sh");
	return NULL;
}

void* thread_fetch(void *arg)
{
	cout<<"fetch"<<endl;
	system("./fetch.sh");
	return NULL;
}

void handleMsg(int sock)
{
	cout<<"handleMsg"<<endl;
	char szbuf[1024];
	while(true)
	{
		memset(szbuf,'\0',1024);
		if(0 == recv(sock,szbuf,1,0))
		{
			close(sock);
			return ;
		}
		cout<<" msg : "<<szbuf[0]<<endl;
		switch(szbuf[0])
		{
			case 'r':
				pthread_t prid;
				pthread_create(&prid,NULL,&thread_run,NULL);
				// control the machine to run the zyzyvva's service process
				break;
			case 'k':
				pthread_t pkid;
				pthread_create(&pkid,NULL,&thread_kill,NULL);
				// stop zyzyvva's service process
				break;
			case 'f':
				pthread_t pfid;
				pthread_create(&pfid,NULL,&thread_fetch,NULL);
				// first, stop the zyzyvva's service process, and then, fetch update .h and .cc
				break;
			case 's':
				cout<<"stop"<<endl;
				close(sock);
				return;
			default:
				return;
		}
	}
}

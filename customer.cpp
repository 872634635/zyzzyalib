//this is a customer .cpp
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

#define SERV_PORT 22223

int main()
{
	vector<string>vec_service;
	ifstream fip("serviceip");
	string servip;
	while(fip>>servip)
	{
		cout<<servip<<endl;
		vec_service.push_back(servip);
	}
	vector<int>vec_socketconnect(vec_service.size(),-1);
	for(int i= 0; i<vec_service.size(); i++)
	{
		struct sockaddr_in servaddr;
		memset(&servaddr,'\0',sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(SERV_PORT);
		inet_pton(AF_INET,vec_service[i].c_str(),&servaddr.sin_addr);
		// the socket function will create a new i-node, and return the index of the i-node.
		vec_socketconnect[i] = socket(AF_INET,SOCK_STREAM,0);
		// the connect function will block until overtime or connect successful.
		if(0 != connect(vec_socketconnect[i],(sockaddr *)&servaddr,sizeof(servaddr)))
		{
			cout<<"the service : " << i<<"  failed"<<endl;
		}
	}				
	while(true)
	{
		
		string strcmd;
		cin>>strcmd;
		if(strcmd[0] != "r" && strcmd[0] != "k" && strcmd[0] != "f" && strcmd[0] != "s")
		{
			continue;
		}
		int index = -1;
		if(strcmd.size() > 1)
		{
			if(strcmd.size() > 2)
				continue;
			else
				index = 0 + (strcmd[1]-'0');
		}
		cout<<"strcmd:  "<<strcmd<<endl;
		for(int i = 0; i<vec_service.size(); i++)
		{
			if(index == -1 || (index != -1 && index == i))
				send(vec_socketconnect[i],strcmd.c_str(),1,0);
		}
	}
	for(int i = 0; i<vec_service.size(); i++)
	{
		close(vec_socketconnect[i]);
	}
	return 0;
}

#ifndef _COMMON_H_
#define _COMMON_H_

#include<iostream>

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>

#include<fcntl.h>
#include<sys/stat.h>

#include<signal.h>

#include<sys/epoll.h>

#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include<sys/wait.h>

#include<vector>
#include<map>

using namespace std;

#define ERR_EXIT(m) \
	do{\
	perror(m);\
	exit(EXIT_FAILURE);\
	}while(0)

#define FILE_NAME_MAX_SIZE 128
#define HOST_NAME_MAX_SIZE 128


#endif /* _COMMON_H_ */
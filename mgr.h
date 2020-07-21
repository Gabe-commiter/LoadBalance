#ifndef _MGR_H_
#define _MGR_H_

#include"log.h"
#include"conn.h"
#include"fdwrapper.h"

class host
{
public:
    char m_hostname[HOST_NAME_MAX_SIZE];
    int m_port;
    int m_conncnt;
};

class mgr
{
public:
	mgr(int epollfd, const host &srv);
	~mgr();
public:
	int conn2srv( const sockaddr_in& address );
	conn* pick_conn(int sockfd);
	int get_used_conn_cnt();
	RET_CODE process(int fd, OP_TYPE type);
	void free_conn( conn* connection );
private:
	static int m_epollfd;

	map<int, conn*> m_conns; //已连接
	map<int, conn*> m_used;  //已使用
	map<int, conn*> m_freed; //未使用

	host m_logic_srv;
};

#endif /* _MGR_H_ */
#ifndef _CONN_H_
#define _CONN_H_

#include"common.h"
#include"fdwrapper.h"

class conn
{
public:
	conn();
	~conn();
public:
	void init_clt(int sockfd, const struct sockaddr_in &client_addr);
	void init_srv(int sockfd, const struct sockaddr_in &server_addr);
	void reset();

	RET_CODE read_clt();
    RET_CODE write_clt();
    RET_CODE read_srv();
    RET_CODE write_srv();
public:
	static const int BUF_SIZE = 2048;
	//客户端信息
	char *m_clt_buf;
	int   m_clt_read_idx;
	int   m_clt_write_idx;
	struct sockaddr_in m_clt_address;
	int   m_cltfd;

	//服务端信息
	char *m_srv_buf;
	int   m_srv_read_idx;
	int   m_srv_write_idx;
	struct sockaddr_in m_srv_address;
	int   m_srvfd;

	bool m_srv_closed;
};

#endif /* _CONN_H_ */
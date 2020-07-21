#ifndef _PROCESS_POOL_H_
#define _PROCESS_POOL_H_

#include"fdwrapper.h"
#include"conn.h"

class process
{
public:
	process() : m_pid(-1)
	{}
public:
	int m_busy_ratio; //负载系数
	pid_t m_pid;
	int m_pipefd[2];
};

static int sig_pipefd[2];
static void sig_handler( int sig )
{
	printf("sig = %d\n",sig);
    int save_errno = errno;
    int msg = sig;
    send( sig_pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}
static void addsig( int sig, void (*handler)(int), bool restart = true )
{
	signal(sig, handler);
	/*
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    if( restart )
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
	*/
}

//进程池       conn        host         mgr
template<typename C, typename H, typename M>
class processpool
{
public:
	static processpool<C,H,M>* create(int listenfd, int process_number = 8)
	{
		if(!m_instance)
		{
			m_instance = new processpool<C,H,M>(listenfd, process_number);
		}
		return m_instance;
	}
public:
	~processpool()
	{
		delete []m_sub_process;
	}
	void run(const vector<H> &arg);
private:
	void run_child(const vector<H> &arg);
	void run_parent();
private:
	void setup_sig_pipe();
	int get_most_free_srv();
	void notify_parent_busy_ratio(int pipefd, M *manager);
private:
	processpool(int listenfd, int process_number=8);
private:
	static const int MAX_PROCESS_NUMBER = 16;
	static const int USER_PER_PROCESS = 65536;
	static const int MAX_EVENT_NUMBER = 10000;

	int m_process_number;
	int m_idx;
	int m_epollfd;
	int m_listenfd;
	int m_stop;
	process *m_sub_process;
	static processpool<C,H,M> *m_instance;
};

template<typename C, typename H, typename M>
processpool<C,H,M>* processpool<C,H,M>::m_instance = NULL;

template<typename C, typename H, typename M>
processpool<C,H,M>::processpool(int listenfd, int process_number)
	:m_listenfd(listenfd), m_process_number(process_number), m_idx(-1),m_stop(false)
{
	assert((process_number>0) && (process_number<=MAX_PROCESS_NUMBER));
	m_sub_process = new process[process_number];
	assert(m_sub_process);

	for(int i=0; i<process_number; ++i)
	{
		int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
		assert(ret == 0);
		m_sub_process[i].m_pid = fork();
		assert(m_sub_process[i].m_pid >= 0);
		if(m_sub_process[i].m_pid > 0)
		{
			close(m_sub_process[i].m_pipefd[1]);
			m_sub_process[i].m_busy_ratio = 0;
			continue;
		}
		else
		{
			close(m_sub_process[i].m_pipefd[0]);
			m_idx = i;
			break;
		}
	}
}

template<typename C, typename H, typename M>
void processpool<C,H,M>::setup_sig_pipe()
{
	m_epollfd = epoll_create( 5 );
    assert( m_epollfd != -1 );

    int ret = socketpair( PF_UNIX, SOCK_STREAM, 0, sig_pipefd );
    assert( ret != -1 );

    setnonblocking( sig_pipefd[1] );
    add_read_fd( m_epollfd, sig_pipefd[0] );

    addsig( SIGCHLD, sig_handler );
    addsig( SIGTERM, sig_handler );
    addsig( SIGINT,  sig_handler );
    addsig( SIGPIPE, SIG_IGN );
}

template<typename C, typename H, typename M>
int processpool<C,H,M>::get_most_free_srv()
{
	int ratio = m_sub_process[0].m_busy_ratio;
	int idx = 0;
	for(int i=1; i<m_process_number; ++i)
	{
		if(m_sub_process[i].m_busy_ratio < ratio)
		{
			idx = i;
			ratio = m_sub_process[i].m_busy_ratio;
		}
	}
	return idx;
}

template<typename C, typename H, typename M>
void processpool<C,H,M>::notify_parent_busy_ratio(int pipefd, M *manager)
{
	int msg = manager->get_used_conn_cnt();
	printf("msg = %d\n", msg);
	send(pipefd, (char *)&msg, 1, 0);
}

//////////////////////////////////////////////////////////////////////


template<typename C, typename H, typename M>
void processpool<C,H,M>::run(const vector<H> &arg)
{
	if(m_idx != -1)
	{
		run_child(arg);
		return;
	}
	run_parent();
}

template<typename C, typename H, typename M>
void processpool<C,H,M>::run_child(const vector<H> &arg)
{
	setup_sig_pipe();
	int pipefd_read = m_sub_process[m_idx].m_pipefd[1];
	add_read_fd(m_epollfd, pipefd_read);

	/////////////////////////////////////////////////////////////////
	M *manager = new M(m_epollfd, arg[m_idx]);
	/////////////////////////////////////////////////////////////////

	struct epoll_event revents[MAX_EVENT_NUMBER];
	int number = 0;
	while(!m_stop)
	{
		number = epoll_wait(m_epollfd, revents, MAX_EVENT_NUMBER, -1);
		if((number < 0) && (errno != EINTR))
		{
			printf("%s\n", "epoll failure");
			break;
		}
		if(number == 0)
		{
			printf("%s\n", "epoll timed out.");
			continue;
		}

		int ret;
		for(int i=0; i<number; ++i)
		{
			//1 io处理
			//2 信号处理
			int sockfd = revents[i].data.fd;
			if((sockfd==pipefd_read) && (revents[i].events&EPOLLIN))
			{
				int client = 0;
				ret = recv(sockfd, (char*)&client, sizeof(client), 0);
				if(ret < 0)
				{
					continue;
				}
				else
				{
					//客户端连接处理
					struct sockaddr_in client_address;
					socklen_t addrlen = sizeof(client_address);
					int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &addrlen);
					if(connfd < 0)
					{
						printf("accept error, errno: %s\n",strerror(errno));
						continue;
					}
					//add_read_fd(m_epollfd, connfd);
					//更新管理类
					C *conn = manager->pick_conn(connfd);
					if(!conn)
					{
						closefd(m_epollfd, connfd);
						continue;
					}

					conn->init_clt(connfd, client_address);
					notify_parent_busy_ratio(pipefd_read, manager);
				}
			}
			else if( ( sockfd == sig_pipefd[0] ) && ( revents[i].events & EPOLLIN ) )
            {
                int sig;
                char signals[1024];
                ret = recv( sig_pipefd[0], signals, sizeof( signals ), 0 );
                if( ret <= 0 )
                {
                    continue;
                }
                else
                {
                    for( int i = 0; i < ret; ++i )
                    {
                        switch( signals[i] )
                        {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
                                {
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                m_stop = true;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }
			else if(revents[i].events & EPOLLIN)
			{
				RET_CODE result = manager->process(sockfd, READ);
				switch(result)
				{
					case CLOSED:
						notify_parent_busy_ratio(pipefd_read, manager);
						break;
					default:
						break;
				}
			}
			else if(revents[i].events & EPOLLOUT)
			{
				RET_CODE result = manager->process(sockfd, WRITE);
				switch(result)
				{
					case CLOSED:
						notify_parent_busy_ratio(pipefd_read, manager);
						break;
					default:
						break;
				}
			}
			
		}
	}
	close(pipefd_read);
	close(m_epollfd);
}

template<typename C, typename H, typename M>
void processpool<C,H,M>::run_parent()
{
	setup_sig_pipe();
	for(int i=0; i<m_process_number; ++i)
	{
		add_read_fd(m_epollfd, m_sub_process[i].m_pipefd[0]);
	}
	add_read_fd(m_epollfd, m_listenfd);

	struct epoll_event revents[MAX_EVENT_NUMBER];
	int number = 0;
	int ret;
	while(!m_stop)
	{
		number = epoll_wait(m_epollfd, revents, MAX_EVENT_NUMBER, -1);
		if((number < 0) && (errno != EINTR))
		{
			printf("%s\n", "epoll failure");
			break;
		}
		if(number == 0)
		{
			printf("%s\n", "epoll timed out.");
			continue;
		}
		int new_conn = 1;
		for(int i=0; i<number; ++i)
		{
			int sockfd = revents[i].data.fd;
			if(sockfd == m_listenfd)
			{
				//1 客户连接
				int idx = get_most_free_srv();
				send(m_sub_process[idx].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
				printf("send request to child %d\n", idx);
			}
			else if( ( sockfd == sig_pipefd[0] ) && ( revents[i].events & EPOLLIN ) )
            {
                int sig;
                char signals[1024];
                ret = recv( sig_pipefd[0], signals, sizeof( signals ), 0 );
                if( ret <= 0 )
                {
                    continue;
                }
                else
                {
                    for( int i = 0; i < ret; ++i )
                    {
                        switch( signals[i] )
                        {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
                                {
                                    for( int i = 0; i < m_process_number; ++i )
                                    {
                                        if( m_sub_process[i].m_pid == pid )
                                        {
                                            //log( LOG_INFO, __FILE__, __LINE__, "child %d join", i );
											printf( "child %d join\n", i);
                                            close( m_sub_process[i].m_pipefd[0] );
                                            m_sub_process[i].m_pid = -1;
                                        }
                                    }
                                }
                                m_stop = true;
                                for( int i = 0; i < m_process_number; ++i )
                                {
                                    if( m_sub_process[i].m_pid != -1 )
                                    {
                                        m_stop = false;
                                    }
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                //log( LOG_INFO, __FILE__, __LINE__, "%s", "kill all the clild now" );
								printf("%s\n", "kill all the clild now");
                                for( int i = 0; i < m_process_number; ++i )
                                {
                                    int pid = m_sub_process[i].m_pid;
                                    if( pid != -1 )
                                    {
                                        kill( pid, SIGTERM );
                                    }
                                }
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }
			else if(revents[i].events & EPOLLIN)
			{
				int busy_ratio = 0;
				ret = recv(sockfd, (char*)&busy_ratio, sizeof(busy_ratio), 0);
				if(ret < 0)
					continue;
				for(int i=0; i<m_process_number; ++i)
				{
					if(sockfd == m_sub_process[i].m_pipefd[0])
					{
						printf("busy_ratio = %d\n", busy_ratio);
						m_sub_process[i].m_busy_ratio = busy_ratio;
						break;
					}
				}
				continue;
			}
			
		}
	}

	for(int i=0; i<m_process_number; ++i)
	{
		closefd(m_epollfd, m_sub_process[i].m_pipefd[0]);
	}
	close(m_epollfd);
}

#endif /* _PROCESS_POOL_H_ */
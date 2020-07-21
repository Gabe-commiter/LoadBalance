#include"mgr.h"

int mgr::m_epollfd = -1;

mgr::mgr(int epollfd, const host &srv) : m_logic_srv(srv)
{
	m_epollfd = epollfd;
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(srv.m_port);  //port
	address.sin_addr.s_addr = inet_addr(srv.m_hostname); //ip
	//log( LOG_INFO, __FILE__, __LINE__, "logcial srv host info: (%s, %d)", srv.m_hostname, srv.m_port );
	//printf("logcial srv host info: (%s, %d)\n", srv.m_hostname, srv.m_port);
	LOG_TRACE("logcial srv host info: (%s, %d)", srv.m_hostname, srv.m_port);
	
	for(int i=0; i<srv.m_conncnt; ++i)
	{
		sleep(1);
		int sockfd = conn2srv(address);
		if( sockfd < 0 )
        {
            //log( LOG_ERR, __FILE__, __LINE__, "build connection %d failed", i );
			//printf("build connection %d failed\n", i);
			LOG_TRACE("build connection %d failed", i);
        }
        else
        {
            //log( LOG_INFO, __FILE__, __LINE__, "build connection %d to server success", i );
			//printf("%d build connection [%d] to server success\n", i, sockfd);
			LOG_TRACE("%d build connection [%d] to server success", i, sockfd);
           
			conn* tmp = NULL;
            tmp = new conn;  //?????????????????
            tmp->init_srv( sockfd, address );
            m_conns.insert( pair< int, conn* >( sockfd, tmp ) );
        }
	}
}

mgr::~mgr()
{}

/////////////////////////////////////////////////////////////////////////////////
int mgr::conn2srv( const sockaddr_in& address )
{
    int sockfd = socket( AF_INET, SOCK_STREAM, 0 );
    if( sockfd < 0 )
    {
        return -1;
    }

    if ( connect( sockfd, ( struct sockaddr* )&address, sizeof( address ) ) != 0  )
    {
        close( sockfd );
        return -1;
    }
    return sockfd;
}

conn* mgr::pick_conn(int cltfd)
{
	if(m_conns.empty())
	{
		printf("%s\n", "not enough srv connections to server.");
		return NULL;
	}
	map<int, conn*>::iterator iter = m_conns.begin();
	int srvfd = iter->first;
	conn *tmp = iter->second;

	if(!tmp)
	{
		printf("%s\n", "empty server connection object.");
		return	NULL;
	}
	
	m_conns.erase(iter);
	m_used.insert(pair<int, conn*>(cltfd, tmp));
	m_used.insert(pair<int, conn*>(srvfd, tmp));

	add_read_fd(m_epollfd, cltfd);
	add_read_fd(m_epollfd, srvfd);

	printf("bind client sock %d with server sock %d\n", cltfd, srvfd);
	return tmp;
}

void mgr::free_conn( conn* connection )
{
    int cltfd = connection->m_cltfd;
    int srvfd = connection->m_srvfd;
    closefd( m_epollfd, cltfd );
    closefd( m_epollfd, srvfd );
    m_used.erase( cltfd );
    m_used.erase( srvfd );
    connection->reset();
    m_freed.insert( pair< int, conn* >( srvfd, connection ) );
}

int mgr::get_used_conn_cnt()
{
	return m_used.size();
}


RET_CODE mgr::process(int fd, OP_TYPE type)
{
	conn *connection = m_used[fd];
	if(!connection)
	{
		return NOTHING;
	}
	if(connection->m_cltfd == fd)
	{
		int srvfd = connection->m_srvfd;
		switch(type)
		{
			case READ:
			{
				RET_CODE res = connection->read_clt();	
				switch( res )
                {
                    case OK:
                    {
                        //log( LOG_DEBUG, __FILE__, __LINE__, "content read from client: %s", connection->m_clt_buf );
						printf("content read from client: %s\n", connection->m_clt_buf);
                    }
                    case BUFFER_FULL:
                    {
                        modfd( m_epollfd, srvfd, EPOLLOUT );
                        break;
                    }
                    case IOERR:
                    case CLOSED:
                    {
                        free_conn( connection );
                        return CLOSED;
                    }
                    default:
                        break;
                }
                if( connection->m_srv_closed )
                {
                    free_conn( connection );
                    return CLOSED;
                }
                break;
			}	

			case WRITE:
			{
                RET_CODE res = connection->write_clt();
                switch( res )
                {
                    case TRY_AGAIN:
                    {
                        modfd( m_epollfd, fd, EPOLLOUT );
                        break;
                    }
                    case BUFFER_EMPTY:
                    {
                        modfd( m_epollfd, srvfd, EPOLLIN );
                        modfd( m_epollfd, fd, EPOLLIN );
                        break;
                    }
                    case IOERR:
                    case CLOSED:
                    {
                        free_conn( connection );
                        return CLOSED;
                    }
                    default:
                        break;
                }
                if( connection->m_srv_closed )
                {
                    free_conn( connection );
                    return CLOSED;
                }
                break;
            }
            default:
            {
                //log( LOG_ERR, __FILE__, __LINE__, "%s", "other operation not support yet" );
				printf( "%s", "other operation not support yet");
                break;
            }
		}
	}
	else if(connection->m_srvfd == fd)
	{
		 int cltfd = connection->m_cltfd;
        switch( type )
        {
            case READ:
            {
                RET_CODE res = connection->read_srv();
                switch( res )
                {
                    case OK:
                    {
                        //log( LOG_DEBUG, __FILE__, __LINE__, "content read from server: %s", connection->m_srv_buf );
						printf( "content read from server: %s\n", connection->m_srv_buf);
                    }
                    case BUFFER_FULL:
                    {
                        modfd( m_epollfd, cltfd, EPOLLOUT );
                        break;
                    }
                    case IOERR:
                    case CLOSED:
                    {
                        modfd( m_epollfd, cltfd, EPOLLOUT );
                        connection->m_srv_closed = true;
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
            case WRITE:
            {
                RET_CODE res = connection->write_srv();
                switch( res )
                {
                    case TRY_AGAIN:
                    {
                        modfd( m_epollfd, fd, EPOLLOUT );
                        break;
                    }
                    case BUFFER_EMPTY:
                    {
                        modfd( m_epollfd, cltfd, EPOLLIN );
                        modfd( m_epollfd, fd, EPOLLIN );
                        break;
                    }
                    case IOERR:
                    case CLOSED:
                    {
                        modfd( m_epollfd, cltfd, EPOLLOUT );
                        connection->m_srv_closed = true;
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
            default:
            {
                //log( LOG_ERR, __FILE__, __LINE__, "%s", "other operation not support yet" );
				printf( "%s\n", "other operation not support yet");
                break;
            }
        }
	}
	else
    {
        return NOTHING;
    }
    return OK;
}
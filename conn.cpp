#include"conn.h"

conn::conn()
{
	m_clt_buf = new char[BUF_SIZE];
	m_srv_buf = new char[BUF_SIZE];
	if(m_clt_buf==NULL || m_srv_buf==NULL)
		ERR_EXIT("conn");
	reset();
}
conn::~conn()
{
	delete []m_clt_buf;
	delete []m_srv_buf;
}

void conn::reset()
{
	m_clt_read_idx = 0;
	m_clt_write_idx = 0;
	m_cltfd = -1;

	m_srv_read_idx = 0;
	m_srv_write_idx = 0;
	m_srvfd = -1;

	m_srv_closed = false;

	memset(m_clt_buf, 0, BUF_SIZE);
	memset(m_srv_buf, 0, BUF_SIZE);
}

void conn::init_srv(int sockfd, const struct sockaddr_in &server_addr)
{
	m_srvfd = sockfd;
	m_srv_address = server_addr;
}

void conn::init_clt(int sockfd, const struct sockaddr_in &client_addr)
{
	m_cltfd = sockfd;
	m_clt_address = client_addr;
}

RET_CODE conn::read_clt()
{
    int bytes_read = 0;
    while( true )
    {
        if( m_clt_read_idx >= BUF_SIZE )
        {
            //log( LOG_ERR, __FILE__, __LINE__, "%s", "the client read buffer is full, let server write" );
			printf( "%s\n", "the client read buffer is full, let server write");
            return BUFFER_FULL;
        }

        bytes_read = recv( m_cltfd, m_clt_buf + m_clt_read_idx, BUF_SIZE - m_clt_read_idx, 0 );
        if ( bytes_read == -1 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                break;
            }
            return IOERR;
        }
        else if ( bytes_read == 0 )
        {
            return CLOSED;
        }

        m_clt_read_idx += bytes_read;
    }
    return ( ( m_clt_read_idx - m_clt_write_idx ) > 0 ) ? OK : NOTHING;
}

RET_CODE conn::read_srv()
{
    int bytes_read = 0;
    while( true )
    {
        if( m_srv_read_idx >= BUF_SIZE )
        {
            //log( LOG_ERR, __FILE__, __LINE__, "%s", "the server read buffer is full, let client write" );
			printf("%s\n", "the server read buffer is full, let client write");
            return BUFFER_FULL;
        }

        bytes_read = recv( m_srvfd, m_srv_buf + m_srv_read_idx, BUF_SIZE - m_srv_read_idx, 0 );
        if ( bytes_read == -1 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                break;
            }
            return IOERR;
        }
        else if ( bytes_read == 0 )
        {
            //log( LOG_ERR, __FILE__, __LINE__, "%s", "the server should not close the persist connection" );
			printf("%s\n", "the server should not close the persist connection");
            return CLOSED;
        }

        m_srv_read_idx += bytes_read;
    }
    return ( ( m_srv_read_idx - m_srv_write_idx ) > 0 ) ? OK : NOTHING;
}

RET_CODE conn::write_srv()
{
    int bytes_write = 0;
    while( true )
    {
        if( m_clt_read_idx <= m_clt_write_idx )
        {
            m_clt_read_idx = 0;
            m_clt_write_idx = 0;
            return BUFFER_EMPTY;
        }

        bytes_write = send( m_srvfd, m_clt_buf + m_clt_write_idx, m_clt_read_idx - m_clt_write_idx, 0 );
        if ( bytes_write == -1 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                return TRY_AGAIN;
            }
            //log( LOG_ERR, __FILE__, __LINE__, "write server socket failed, %s", strerror( errno ) );
			printf("write server socket failed, %s\n", strerror( errno ));
            return IOERR;
        }
        else if ( bytes_write == 0 )
        {
            return CLOSED;
        }

        m_clt_write_idx += bytes_write;
    }
}
RET_CODE conn::write_clt()
{
    int bytes_write = 0;
    while( true )
    {
        if( m_srv_read_idx <= m_srv_write_idx )
        {
            m_srv_read_idx = 0;
            m_srv_write_idx = 0;
            return BUFFER_EMPTY;
        }

        bytes_write = send( m_cltfd, m_srv_buf + m_srv_write_idx, m_srv_read_idx - m_srv_write_idx, 0 );
        if ( bytes_write == -1 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                return TRY_AGAIN;
            }
            //log( LOG_ERR, __FILE__, __LINE__, "write client socket failed, %s", strerror( errno ) );
			printf("write client socket failed, %s\n", strerror( errno ) );
            return IOERR;
        }
        else if ( bytes_write == 0 )
        {
            return CLOSED;
        }

        m_srv_write_idx += bytes_write;
    }
}

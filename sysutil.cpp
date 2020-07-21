#include"sysutil.h"

const char *version = "1.0";

void Version(const char *v)
{printf("loadbalance: version %s\n",version);}
void Usage(const char *exe)
{printf("Usage: %s [-x] [-v] [-h] [-f cfgfile]\n", exe);}

void ParseCmdArg(int argc, char *argv[], char *cfgfile)
{
	int option;
	const char *optstring = "f:xvh";
	while((option = getopt(argc, argv, optstring)) != -1)
	{
		switch(option)
		{
		case 'x':
			break;
		case 'v':
			Version(version);
			break;
		case 'h':
			Usage(basename(argv[0]));
			break;
		case 'f':
			strcpy(cfgfile, optarg);
			break;
		case '?':
			break;
		}
	}
}

int LoadCfgFile(const char *filename, char *&buf)
{
	if(*filename == '\0')
		return -1;
	int fd;
	if((fd = open(filename, O_RDONLY)) < 0)
		return -1;
	
	struct stat statbuf;
	if(fstat(fd, &statbuf) < 0)
		return -1;

	size_t file_len = statbuf.st_size;
	buf = new char [file_len+1];

	if(read(fd, buf, file_len) < 0)
		return -1;

	close(fd);
	return 0;
}

int ParseCfgFile(const char *filename, vector<host> &balance_srv, vector<host> &logical_srv)
{
	TiXmlDocument doc;  
    if(!doc.LoadFile(filename))
		return -1;

	host tmp_host;
	memset(&tmp_host, 0, sizeof(tmp_host));

	TiXmlElement* host = doc.RootElement();
	TiXmlElement* balance = host->FirstChildElement();
	//½âÎöBalance Server
	TiXmlElement* balance_host = balance->FirstChildElement();
	TiXmlElement* hostElement = balance_host->FirstChildElement();
	for(; hostElement!=NULL; hostElement=hostElement->NextSiblingElement())
	{
		//cout<<hostElement->Value()<<":"<<hostElement->GetText()<<endl;
		if(strcmp(hostElement->Value(), "ip") == 0)
		{
			strcpy(tmp_host.m_hostname, hostElement->GetText());
		}
		else if(strcmp(hostElement->Value(), "port") == 0)
		{
			tmp_host.m_port = atoi(hostElement->GetText());
		}
	}
	balance_srv.push_back(tmp_host);

	TiXmlElement* logical = balance->NextSiblingElement();
	TiXmlElement* logical_host = logical->FirstChildElement();
	for(; logical_host!=NULL; logical_host=logical_host->NextSiblingElement())
	{
		memset(&tmp_host, 0, sizeof(tmp_host));
		hostElement = logical_host->FirstChildElement();
		for(; hostElement!=NULL; hostElement=hostElement->NextSiblingElement())
		{
			//cout<<hostElement->Value()<<":"<<hostElement->GetText()<<endl;
			if(strcmp(hostElement->Value(), "ip") == 0)
			{
				strcpy(tmp_host.m_hostname, hostElement->GetText());
			}
			else if(strcmp(hostElement->Value(), "port") == 0)
			{
				tmp_host.m_port = atoi(hostElement->GetText());
			}
			else if(strcmp(hostElement->Value(), "conns") == 0)
			{
				tmp_host.m_conncnt = atoi(hostElement->GetText());
			}
		}
		logical_srv.push_back(tmp_host);
	}
	return 0;
}


int tcp_server(const char *ip, short port)
{
	int listenfd;
	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		ERR_EXIT("tcp_server: socket");

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = inet_addr(ip);
	
	int on = 1;
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		ERR_EXIT("tcp_server: setsockopt");

	socklen_t addrlen = sizeof(address);
	if(bind(listenfd, (struct sockaddr*)&address, addrlen) < 0)
		ERR_EXIT("tcp_server: bind");

	if(listen(listenfd, 5) < 0)
		ERR_EXIT("tcp_server: listen");

	return listenfd;
}
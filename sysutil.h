#ifndef _SYSUTIL_H_
#define _SYSUTIL_H_

#include"common.h"
#include"log.h"
#include"tinyxml.h"
#include"mgr.h"
#include"processpool.h"

void Version(const char *v);
void Usage(const char *exe);
void ParseCmdArg(int argc, char *argv[], char *cfgfile);
int LoadCfgFile(const char *filename, char *&buf);
int ParseCfgFile(const char *filename, vector<host> &balance_srv, vector<host> &logical_srv);

int tcp_server(const char *ip, short port);

//////////////////////////////////////////////////////////////////////////////////////////////


#endif /* _SYSUTIL_H_ */
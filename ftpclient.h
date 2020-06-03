#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

void ftpClearSocket(int sock);//清空socket
ssize_t ftpRead(int fd,void* buffer,size_t bytes);//读
ssize_t ftpWrite(int fd,const void* buffer,size_t bytes);//写
int serverConnect(const char* ip,uint16_t port);
void ftpClose(int sock);
int ftpControlConnection(const char* ip,uint16_t port);
int ftpDataConnection(int sock);
int ftpPort(int csock);
int ftpAccept(int socklisten);
int ftpLogin(int sock,const char* username,const char* password);
int ftpCwd(int sock,const char* path);
int ftpDownloadCMD(int csock,const char* file);
int ftpUploadCMD(int csock,const char* file);
int ftpDownloadData(int dsock,const char* file);
int ftpUploadData(int dsock,const char* file);
#endif // FTP_CLIENT_H

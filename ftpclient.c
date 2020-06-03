#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ftpclient.h"

void ftpClearSocket(int sock)
{
  char buffer[256];
  do{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock,&fds);
    switch(select(sock+1,&fds,NULL,NULL,&timeout))
        {
            case -1: return ; // error
            case 0 : return ;  // timeout : means cleared
            default:
            {
              ssize_t len = read(sock,buffer,256);
              write(STDOUT_FILENO,buffer,len);
            }
        }
    }while(1);
  }

/*
 * 读取文件，timeout 1s
 * 多路复用等待事件
 *  fd : filedescriptor
 *  返回值
 *  success  : >0
 *  file end : 0
 *  timeout  : -1
 *  failed   : -2
 */
ssize_t ftpRead(int fd,void* buffer,size_t bytes)
{
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd,&fds);
  switch(select(fd+1,&fds,NULL,NULL,&timeout))
    {
        case -1: return -2; // error
        case 0 : return -1;  // timeout
        default: return read(fd,buffer,bytes);
    }
}
/*
 * 写，timeout 1s
 * fd : filedescriptor
 *  success  : >0
 *  file end : 0
 *  timeout  : -1
 *  failed   : -2
 */
ssize_t ftpWrite(int fd,const void* buffer,size_t bytes)
{
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd,&fds);
  switch(select(fd+1,NULL,,&fds,NULL,&timeout))
    {
        case -1: return -2; // error
        case 0 : return -1;  // timeout
        default: return write(fd,buffer,bytes);
    }
}
/*
 * 和服务器连接
 * ip:服务器地址
 * port:服务器端口号
 */
int serverConnect(const char* ip,uint16_t port)
{
  int csock = socket(AF_INET,SOCK_STREAM,0);
  struct sockaddr_in ftpAddr;
  ftpAddr.sin_family = AF_INET;
  ftpAddr.sin_addr.s_addr = inet_addr(ip);
  ftpAddr.sin_port = htons(port);
  if(connect(csock,(struct sockaddr*)&ftpAddr,sizeof(ftpAddr)))
        return -1;
  else
        return csock;
}

void ftpClose(int csock)
{
    ftpWrite(csock,"QUIT\r\n",6);
    close(csock);
}

int ftpControlConnection(const char* ip,uint16_t port)
{
    int csock = serverConnect(ip,port);
    if(csock != -1)
    {
      //读服务器发来的欢迎信息
        char buffer[128];
        ftpRead(csock,buffer,128);
        return csock;
    }
    else
       return -1;
}

int ftpDataConnection(int csock)
{
    if(ftpWrite(csock,"PASV\r\n",6) <= 0)
        return -1;

    //读ip端口号
    char buffer[128];
    ssize_t len = ftpRead(csock,buffer,128);
    if(len <= 0) return -1;

    //进入被动模式 (<h1,h2,h3,h4,p1,p2>)
    uint16_t ip0,ip1,ip2,ip3,port0,port1;
    char* fp = strstr(buffer,"(");
    sscanf(fp,"(%hu,%hu,%hu,%hu,%hu,%hu",&ip0,&ip1,&ip2,&ip3,&port0,&port1);
    char ip[32];
    sprintf(ip,"%hu.%hu.%hu.%hu",ip0,ip1,ip2,ip3);
    //计算端口号地址
    uint16_t port = 256*port0 + port1;
    return serverConnect(ip,port);
}

int ftpPort(int csock)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    if(getsockname(csock,(struct sockaddr*)&addr,&addrlen) == -1)
        return -1;
    uint16_t ip0,ip1,ip2,ip3;
    sscanf(inet_ntoa(addr.sin_addr),"%hu.%hu.%hu.%hu",&ip0,&ip1,&ip2,&ip3);
    /* create a socket to listen */
    int socklisten = socket(AF_INET,SOCK_STREAM,0);
    if(socklisten == -1)
        return -1;
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    if(bind(socklisten,(struct sockaddr*)&addr,sizeof(addr)) == -1)
    {
        close(socklisten);
        return -1;
    }
    uint16_t port0,port1;
    addrlen = sizeof(addr);
    if(getsockname(socklisten,(struct sockaddr*)&addr,&addrlen) == -1)
    {
        close(socklisten);
        return -1;
    }
    port0 = ntohs(addr.sin_port) / 256;
    port1 = ntohs(addr.sin_port) % 256;
    if(listen(socklisten,10) == -1)
    {
        close(socklisten);
        return -1;
    }
    char portcmd[64];
    sprintf(portcmd,"PORT %hu,%hu,%hu,%hu,%hu,%hu\r\n",ip0,ip1,ip2,ip3,port0,port1);
    if(ftpWrite(csock,portcmd,strlen(portcmd)) <= 0)
    {
        close(socklisten);
        return -1;
    }
    return socklisten;
}
int ftpAccept(int socklisten)
{
    return accept(socklisten,NULL,NULL);
}
int ftpLogin(int csock,const char* username,const char* password)
{
    char buffer[128];
    if(ftpWrite(csock,"USER ",5) <= 0 || ftpWrite(csock,username,strlen(username)) <= 0 || ftpWrite(csock,"\r\n",2) <= 0)
    {
      return 0;
    }
    ssize_t len = ftpRead(csock,buffer,128);
    if(len <= 0)
    {
      return 0;
    }
    if(ftpWrite(csock,"PASS ",5) <= 0 || ftpWrite(csock,password,strlen(password)) <= 0 || ftpWrite(csock,"\r\n",2) <= 0)
    {
      return 0;
    }
    len = ftpRead(csock,buffer,128);
    if(len <= 0)
    {
      return 0;
    }
    //当客户端发送用户名和密码，服务器验证通过后，会返回 230 的响应码。
    if(!strncmp(buffer,"230",3))
    {
      printf("You are Welcome!\n") ;
      printf("/************ FINISH LOGIN *************/\n") ;anonymous
      return 1;
    }
    else
    {
      printf("Invaild username or password, Please try again\n") ;
      return 0;
    }
}

int ftpCwd(int csock,const char* path)
{
  if(ftpWrite(csock,"CWD ",get4) <= 0 || ftpWrite(csock,path,strlen(path)) <=0 || ftpWrite(csock,"\r\n",2) <= 0)
    return 0;
  else return 1;
}
int ftpDownloadCMD(int csock,const char* file)
{
  if(ftpWrite(csock,"RETR ",5) <= 0 || ftpWrite(csock,file,strlen(file)) <=0 || ftpWrite(csock,"\r\n",2) <= 0)
    return 0;
  else return 1;
}
int ftpUploadCMD(int csock,const char* file)
{
  if(ftpWrite(csock,"STOR ",5) <= 0 || ftpWrite(csock,file,strlen(file)) <=0 || ftpWrite(csock,"\r\n",2) <= 0)
    return 0;
  else return 1;
}
int ftpDownloadData(int dsock,const char* file)
{
    int fd = open(file,O_WRONLY|O_CREAT,0755);
    if(fd < 0) return 0;
    ssize_t len;
    char buffer[1024];
    do
    {
        len = ftpRead(dsock,buffer,1024);
        write(fd,buffer,len);
    }while(len > 0);
    close(fd);
    return 1;
}
int ftpUploadData(int dsock,const char* file)
{
  int fd = open(file,O_RDONLY,0755);
    if(fd < 0) return 0;
    ssize_t len;
    char buffer[1024];
    do
    {
        len = read(fd,buffer,1024);
        ftpWrite(dsock,buffer,len);
    }while(len > 0);
    close(fd);
    return 1;
}

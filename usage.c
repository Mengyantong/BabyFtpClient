#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ftpclient.h"
int login()
{
    char username[128];
    char* buffer = username;
    uint16_t port;
    const char *short_opts = "i:n:p:h";
    const struct option long_opts[] =   {
            {"help", no_argument, NULL, 'h'},
            {"ip", required_argument, NULL, 'i'},
            { "port", required_argument, NULL, 'p'},
            {0, 0, 0, 0}
        };
    while ((opt= getopt_long(argc, argv, short_opts, long_opts,NULL)) != -1)
    {
        switch (opt)
        {
            case 'i':
                ip = optarg ;
                break ;
            case 'p':
                port = atoi(optarg) ;
                break ;
            case 'h':
                print_usage(program_name) ;
                return 0 ;
        }
     }

    no_ipport = ( (!ip) || (!port) ) ;


    printf("\n/************ START LOGIN *************/\n") ;
    printf("       Username : ");
    scanf("%s",username);
    char* password = getpass("       Password : ");
    if(ftpLogin(csock,username,password)) return csock;
    else return -1;
}
void list(int csock)
{
    int socklisten = ftpPort(csock);
    ftpWrite(csock,"LIST\r\n",6);
    int dsock = ftpAccept(socklisten);

    char buffer[1024];
    ssize_t len = 0;
    do
    {
        len = ftpRead(dsock,buffer,1024);
        write(STDOUT_FILENO,buffer,len);
    }while(len > 0);
    close(dsock);
}
void help()
{
    printf("FTP：ftp客户端（175第一组）\n");
    printf("help              显示帮助\n");
    printf("cd                目录变换\n");
    printf("list              列表\n");
    printf("put               上传\n");
    printf("get               下载\n");
    printf("quit              退出\n");
}
void command(int csock,const char* cmd)
{
    char* param = strstr(cmd," ");
    while(param != NULL && *param == ' ')
      param += 1;
    if(!strncmp(cmd,"quit",4))
    {
        ftpClose(csock);
        exit(0);
    }
    else if(!strncmp(cmd,"help",4)) help();
    else if(!strncmp(cmd,"list",4)) list(csock);
    else if(!strncmp(cmd,"cd",2)) ftpCwd(csock,param);
    else if(!strncmp(cmd,"get",3))
    {
        int socklisten = ftpPort(csock);
        ftpDownloadCMD(csock,param);
        int dsock = ftpAccept(socklisten);
        if(dsock < 0 || !ftpDownloadData(dsock,param))
        {
            printf("Download failed.\n");
            return;
        }
    }
    else if(!strncmp(cmd,"put",3))
    {
        int socklisten = ftpPort(csock);
        ftpUploadCMD(csock,param);
        int dsock = ftpAccept(socklisten);
        if(dsock < 0 || !ftpUploadData(dsock,param))
        {
            printf("Upload failed.\n");
            return;
        }
    }
    else
    {
        printf("Unknow Command.\n");
        help();
    }
}

int main()
{
  int csock = login();
    if(csock < 0)
    {
        printf("Create control connection failed.\n");
        return 1;
    }

    fd_set rfds; // fd_set to read
    struct timeval waitTime; // time to wait
    struct timeval* timeout = &waitTime; // time of timeout

    while(1)
    {
        /* init fd_set */
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO,&rfds);
        FD_SET(csock,&rfds);

        /* timeout */
        waitTime.tv_sec  = 0;
        waitTime.tv_usec = 500000;


        int state = select(csock + 1, &rfds, NULL,NULL,timeout);
        switch(state)
        {
            case 0: timeout = NULL; write(STDOUT_FILENO,"FTP >> ",7); break; // if timeout print "FTP >> ", last time will wait forever

            case -1: return 1;// error

            default:
            {
                timeout = &waitTime;    // last time wait 1000us
                ssize_t length = 0;     // length of data
                char buffer[1024];      // data
                /* read command from stdin */
                if(FD_ISSET(STDIN_FILENO,&rfds))
                {
                    length = 0;
                    length = read(STDIN_FILENO,buffer,1024);
                    buffer[length-1] = 0; // '\n' ==> 0
                    /* execute input command */
                    command(csock,buffer);
                }

                /* read data from socket */
                if(FD_ISSET(csock,&rfds))
                {
                    length = 0;
                    length = read(csock,buffer,1024);
                    if(length <= 0)
                    {
                        printf("FTP server closed connection.\n");
                        return 1;
                    }
                    write(STDOUT_FILENO,buffer,length);
                }
            }
        }
    }
}

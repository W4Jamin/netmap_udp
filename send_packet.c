#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>

#define MAXLINE 1024*10

int main()
{
    int sockfd;
    struct sockaddr_in server_addr;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8686); // port
    server_addr.sin_addr.s_addr = inet_addr(INADDR_ANY); // IP
    char sendline[10]="hello world"; // 
    while(1)
    {
        sendto(sockfd, sendline, strlen(sendline), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        printf("%s\n",sendline);
        sleep(1);
    }
    return 0;
}


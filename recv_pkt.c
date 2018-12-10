#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>

#include <unistd.h>    // sysconf()
#include <sys/poll.h>
#include <arpa/inet.h>

#include "netmap_user.h" /* 来源与netmap */
#pragma pack(1) /* 设置结构体的边界对齐为1个字节 */

struct udp_pkt  /* 普通的完整udp数据包 */
{
    struct ether_header eh;      /* 以太网头部,14字节,<net/ethernet.h>头文件中 */
    struct iphdr ip;              /* ip部分,20字节,<netinet/ip.h>头文件中 */
    struct udphdr udp;             /* udp部分,8字节,<netinet/udp.h>头文件中 */
    uint8_t body[20];            /* 数据部分,暂时分配20字节 */
};

struct arp_pkt /* 完整arp数据包(以太网首部 + ARP字段) */
{
    struct ether_header eh;      /* 以太网头部,14字节,<net/ethernet.h>头文件中 */
    struct ether_arp arp;          /* ARP字段  ,28字节,<netinet/if_ether.h>头文件中 */
};

void Print_mac_addr(const unsigned char *str) /* 打印mac地址 */
{
    int i;
    for (i = 0; i < 5; i++)
        printf("%02x:", str[i]);
    printf("%02x", str[i]);
}

void Print_ip_addr(const unsigned char *str) /* 打印ip地址 */
{
    int i;
    for (i = 0; i < 3; i++)
        printf("%d.", str[i]);
    printf("%d", str[i]);
}

void Print_arp_pkt(struct arp_pkt* arp_pkt) /* 打印完整的arp数据包的内容 */
{
    Print_mac_addr(arp_pkt->eh.ether_dhost), printf(" "); /* 以太网目的地址 */
    Print_mac_addr(arp_pkt->eh.ether_shost), printf(" "); /* 以太网源地址 */
    printf("0x%04x ", ntohs(arp_pkt->eh.ether_type));     /* 帧类型:0x0806 */
    printf("  ");
    printf("%d ",     ntohs(arp_pkt->arp.ea_hdr.ar_hrd));         /* 硬件类型:1 */
    printf("0x%04x ", ntohs(arp_pkt->arp.ea_hdr.ar_pro));         /* 协议类型:0x0800 */
    printf("%d ",arp_pkt->arp.ea_hdr.ar_hln);      /* 硬件地址:6 */
    printf("%d ",arp_pkt->arp.ea_hdr.ar_pln);      /* 协议地址长度:4 */
    printf("%d ",     ntohs(arp_pkt->arp.ea_hdr.ar_op));  /* 操作字段:ARP请求值为1,ARP应答值为2 */
    printf("  ");
    Print_mac_addr(arp_pkt->arp.arp_sha), printf(" ");    /* 发送端以太网地址*/
    Print_ip_addr(arp_pkt->arp.arp_spa), printf(" ");     /* 发送端IP地址 */
    Print_mac_addr(arp_pkt->arp.arp_tha), printf(" ");    /* 目的以太网地址 */
    Print_ip_addr(arp_pkt->arp.arp_tpa), printf(" ");     /* 目的IP地址 */
    printf("\n");
}

/*
 * 根据ARP request生成ARP reply的packet
 * hmac为本机mac地址
 */
void Init_echo_pkt(struct arp_pkt *arp, struct arp_pkt *arp_rt, char *hmac)
{
    bcopy(arp->eh.ether_shost,     arp_rt->eh.ether_dhost, 6);   /* 填入目的地址 */
    bcopy(ether_aton(hmac),     arp_rt->eh.ether_shost, 6);   /* hmac为本机mac地址 */
    arp_rt->eh.ether_type =     arp->eh.ether_type;           /* 以太网帧类型 */
    ;
    arp_rt->arp.ea_hdr.ar_hrd = arp->arp.ea_hdr.ar_hrd;
    arp_rt->arp.ea_hdr.ar_pro = arp->arp.ea_hdr.ar_pro;
    arp_rt->arp.ea_hdr.ar_hln = 6;
    arp_rt->arp.ea_hdr.ar_pln = 4;
    arp_rt->arp.ea_hdr.ar_op = htons(2);                     /* ARP应答 */
    ;
    bcopy(ether_aton(hmac), &arp_rt->arp.arp_sha, 6);        /* 发送端以太网地址*/
    bcopy(arp->arp.arp_tpa, &arp_rt->arp.arp_spa, 4);        /* 发送端IP地址 */
    bcopy(arp->arp.arp_sha, &arp_rt->arp.arp_tha, 6);        /* 目的以太网地址 */
    bcopy(arp->arp.arp_spa, &arp_rt->arp.arp_tpa, 4);        /* 目的IP地址 */
}

int main()
{
    struct ether_header *eh;
    struct nm_pkthdr h;
    struct nm_desc *nmr;
    nmr = nm_open("netmap:enp0s3", NULL, 0, NULL);  /* 打开netmap对应的文件描述符,并做了相关初始化操作！ */
    struct pollfd pfd;
    pfd.fd = nmr->fd;
    pfd.events = POLLIN;
    u_char *str;
    printf("ready!!!\n");
    while (1)
    {
        poll(&pfd, 1, -1); /* 使用poll来监听是否有事件到来 */
        if (pfd.revents & POLLIN)
        {
            str = nm_nextpkt(nmr, &h);                     /* 接收到来的数据包 */
            eh = (struct ether_header *) str;
            if (ntohs(eh->ether_type) == 0x0800)                     /* 接受的是普通IP包 */
            {
                struct udp_pkt *p = (struct udp_pkt *)str;
                if(p->ip.protocol == IPPROTO_UDP)   /*如果是udp协议的数据包*/
                    printf("udp:%s\n", p->body);
                else
                    printf("其它IP协议包!\n");
            }
            else if (ntohs(eh->ether_type) == 0x0806)                  /* 接受的是ARP包 */
            {
                struct arp_pkt arp_rt;
                struct arp_pkt *arp = (struct arp_pkt *)str;
                if( *(uint32_t *)arp->arp.arp_tpa == inet_addr("10.0.2.15") ) /*如果请求的IP是本机IP地址(两边都是网络字节序)*/
                {
                    printf("ARP请求:");
                    Print_arp_pkt(arp);
                    Init_echo_pkt(arp, &arp_rt, "08:00:27:af:d4:07");     /*根据收到的ARP请求生成ARP应答数据包*/
                    printf("ARP应答:");
                    Print_arp_pkt(&arp_rt);
                    nm_inject(nmr, &arp_rt, sizeof(struct arp_pkt));      /* 发送ARP应答包 */
                }
                else
                    printf("其它主机的ARP请求！\n");
            }
        }
    }
    nm_close(nmr);
    return 0;
}


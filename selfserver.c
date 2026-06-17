#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <ifaddrs.h>
#include <stdlib.h>

struct in_addr selfip () {
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *ifa = NULL;
    getifaddrs(&interfaces);
    for (ifa = interfaces; ifa != NULL; ifa = (*ifa).ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *socket_address = (struct sockaddr_in *)ifa->ifa_addr;
            struct in_addr current_ip = socket_address->sin_addr;
            if (current_ip.s_addr != htonl(INADDR_LOOPBACK)) {
                freeifaddrs(interfaces);
                return current_ip; 
            };
        };
    };
}
struct headers {
    struct ip packet;
    struct headers* next;
};
int main () {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    int one = 1;
    setsockopt(sock,IPPROTO_IP,IP_HDRINCL,&one,sizeof(one));
    int size;
    int usernumber = 0;
    while (1) {
        struct ip *iph = malloc(65535*sizeof(char));
        struct headers *firstiphes;
        size = recv(sock,iph,65535*sizeof(char),0);
        if (size == 20) {
            firstiphes=firstiphes-usernumber;
            for (int i = 0; i<usernumber; i++) {
                firstiphes=(*firstiphes).next;
            }
            memcpy(&(*firstiphes).packet, iph, 20);
            usernumber++;
        }
        if (size>20) {
            firstiphes=firstiphes-usernumber;
            for (int i = 0; i < usernumber; i++) { //sending to all the users
                (*iph).ip_dst=(*firstiphes).packet.ip_src;
                iph->ip_len = ntohs(iph->ip_len);
                iph->ip_off = ntohs(iph->ip_off);
                (*iph).ip_src = selfip();
                struct sockaddr_in dst;
                memset(&dst, 0, sizeof(dst));
                dst.sin_len = sizeof(struct sockaddr_in);
                dst.sin_family = AF_INET;
                dst.sin_addr = (*iph).ip_dst;
                dst.sin_port = 0;
                if (sendto(sock, iph, (*iph).ip_len, 0, (struct sockaddr *)&dst, sizeof(dst))<0) {
                    perror("sendto");
                };
                firstiphes=(*firstiphes).next;
            }
        };
        memset(iph, 0, 65535);
        free(iph);
    };
    return 0;
}
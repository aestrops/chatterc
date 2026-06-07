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
int main () {
    struct in_addr *firstiph = malloc(24*sizeof(char));
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    int one = 1;
    setsockopt(sock,IPPROTO_IP,IP_HDRINCL,&one,sizeof(one));
    recv(sock, firstiph, 24*sizeof(char), 0);
    while (1) {
        struct ip *iph = malloc(5200020*sizeof(char));
        if (recv(sock, iph, 5200020*sizeof(char), 0)) {
            if ((*iph).ip_src.s_addr != (*(firstiph+5)).s_addr) {   
                (*iph).ip_dst = *(firstiph+5);
            } else {
                (*iph).ip_dst = *(firstiph+4);
            }
            (*iph).ip_src = selfip();
            struct sockaddr_in dst = {
            .sin_family = AF_INET,
            .sin_addr = (*iph).ip_dst,
            .sin_port = 0,
            };
            if (sendto(sock, iph, (*iph).ip_len, 0, (struct sockaddr *)&dst, sizeof(dst))<0) {
                perror("sendto");
            };
        };
        free(iph);
    };
    free(firstiph);
    return 0;
}
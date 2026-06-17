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
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    int one = 1;
    int firstpacket = 0;
    setsockopt(sock,IPPROTO_IP,IP_HDRINCL,&one,sizeof(one));
    struct in_addr *firstiph;
    while (firstpacket == 0) {
        firstiph = malloc(20*sizeof(char));
        if (recv(sock, firstiph, 20*sizeof(char), MSG_TRUNC) == 20 && *(uint16_t *)(firstiph+1) == htons(52013)) {
            firstpacket = 1;
        } else {
            free(firstiph);
        }
    }
    int secondpacket = 0;
    struct in_addr *secondiph;
    while (secondpacket == 0) {
        secondiph = malloc(20*sizeof(char));
        if (recv(sock, secondiph, 20*sizeof(char), MSG_TRUNC) == 20 && *(uint16_t *)(secondiph+1) == htons(52013) && (*(secondiph+3)).s_addr != (*(firstiph+3)).s_addr) {
            secondpacket = 1;
        } else {
            free(secondiph);
        }
    }
    while (1) {
        struct ip *iph = malloc(65535*sizeof(char));
        int our = 0;
        if (recv(sock, iph, 65535*sizeof(char), 0)) {
            if ((*iph).ip_src.s_addr == (*(firstiph+3)).s_addr && (*iph).ip_id == htons(52013)) {   
                (*iph).ip_dst = *(secondiph+3);
                our = 1;
            } else if ((*iph).ip_src.s_addr == (*(secondiph+3)).s_addr && (*iph).ip_id == htons(52013)) {
                (*iph).ip_dst = *(firstiph+3);
                our = 1;
            }
            if (our == 1) {
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
            }
        };
        memset(iph+1, 0, 65515);
        free(iph);
    };
    free(firstiph);
    free(secondiph);
    return 0;
}
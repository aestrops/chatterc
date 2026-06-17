#include <_types/_uint32_t.h>
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
struct in_addr translate (char *untranslated) {
    struct in_addr translated;
    unsigned int a,b,c,d;
    sscanf(untranslated, "%u.%u.%u.%u", &a,&b,&c,&d);
    uint32_t host_ordered_ip = (a << 24) | (b << 16) | (c << 8) | d;
    uint32_t packet_ready_ip = htonl(host_ordered_ip);
    translated.s_addr=packet_ready_ip;
    return translated;
}
unsigned short calculate_checksum(unsigned short *addr, int count) {
    register long sum = 0;

    // Sum up 16-bit chunks
    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }

    // If there is a leftover byte (odd header length), add it
    if (count > 0) {
        sum += *(unsigned char *)addr;
    }

    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    // Return the one's complement (bit-flipped) result
    return (unsigned short)(~sum);
}

int main (int argc, char *archv[]) {
    //struct in_addr g=selfip();
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    int one = 1;
    setsockopt(sock,IPPROTO_IP,IP_HDRINCL,&one,sizeof(one));
    struct ip *iph = malloc(sizeof(struct ip));
    //initialize header
    (*iph).ip_hl = 5;
    (*iph).ip_v = 4;
    (*iph).ip_ttl = 64;
    (*iph).ip_p = IPPROTO_RAW;
    (*iph).ip_src = selfip();
    (*iph).ip_dst = translate(archv[1]);
    (*iph).ip_len = htons(sizeof(struct ip));
    (*iph).ip_tos = 0x88;
    (*iph).ip_id = htons(52014);
    (*iph).ip_off = 0;
    (*iph).ip_sum = 0;
    (*iph).ip_sum = calculate_checksum((unsigned short *) iph, (*iph).ip_hl * 4);
    (*iph).ip_len = sizeof(struct ip);
    struct sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_addr = translate(archv[1]),
        .sin_port = 0,
    };
    ssize_t sent = sendto(
        sock,                          // the raw socket
        iph,                           // pointer to start of buffer (header + payload)
        (*iph).ip_len,          // total bytes to send, converted back to host order
        0,                             // flags, 0 = none
        (struct sockaddr *)&dst,       // destination — kernel uses this to pick the interface
        sizeof(dst)
    );
    if (sent < 0) {
        perror("sendto");
    };
    free(iph);
    uint32_t source;
    uint32_t dest;
    int lastrcv = 0;
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);   // fd 0
        FD_SET(sock, &readfds);

        select(sock+1, &readfds, NULL, NULL, NULL);  // blocks until one is ready

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char *input = malloc(65535*sizeof(char));
            if (fgets(input+20, 65515*sizeof(char), stdin)) {
                struct ip *iph = input;
                (*iph).ip_hl = 5;
                (*iph).ip_v = 4;
                (*iph).ip_ttl = 64;
                (*iph).ip_p = IPPROTO_RAW;
                (*iph).ip_src = selfip();
                (*iph).ip_dst = translate(archv[1]);
                (*iph).ip_len = htons(strlen(input+20)+sizeof(struct ip));
                (*iph).ip_tos = 0x88;
                (*iph).ip_id = htons(52013);
                (*iph).ip_off = 0;
                (*iph).ip_sum = 0;
                (*iph).ip_sum = calculate_checksum((unsigned short *) iph, (*iph).ip_hl * 4);
                (*iph).ip_len = strlen(input+20)+sizeof(struct ip);
                if (sendto(sock, iph, (*iph).ip_len, 0, (struct sockaddr *)&dst, sizeof(dst))<0) {
                    perror("sendto");
                };
                lastrcv = 0;
                free(input);
            };
        };

        if (FD_ISSET(sock, &readfds)) {
            char *rcvpacket = malloc(65535*sizeof(char));
            if (recv(sock, rcvpacket, 65535*sizeof(char), 0)) {
                source = *(uint32_t *)(rcvpacket+12);
                dest = *(uint32_t *)(rcvpacket+16);
                if (source == translate(archv[1]).s_addr && dest == selfip().s_addr && *(uint16_t *)(rcvpacket+4) == htons(52013)) {
                    if (lastrcv == 1) {
                        printf("%s\n", rcvpacket+20);
                    } else {
                        printf("\n%s\n", rcvpacket+20);
                        lastrcv = 1;
                    }
                    
                }
            };
            free(rcvpacket);
        };

    };
    return 0;
}
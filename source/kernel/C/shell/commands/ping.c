#include <commands/commands.h>
#include <graphics.h>
#include <net/net.h>
int cmd_ping(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: ping <ip-or-host>");
        return 1;
    }
    net_ipv4_t ip;
    if (dns_resolve(argv[1], &ip) != NET_OK) {
        printf("ping: cannot resolve %s", argv[1]);
        return 1;
    }
    char s[20];
    net_format_ipv4(ip, s, sizeof(s));
    printf("PING %s", s);
    for (int i = 0; i < 4; i++) {
        int r = icmp_ping(ip, 0x4242, (uint16)i, 500000);
        printf(r == NET_OK ? "reply from %s" : "timeout from %s", s);
    }
    return 0;
}

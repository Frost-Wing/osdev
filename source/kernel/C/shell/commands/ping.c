/**
 * @file ping.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2026-07-05
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#include <commands/commands.h>
#include <graphics.h>
#include <net/net.h>
#include <pit.h>

extern volatile uint64_t pit_ticks;

#define PING_COUNT    4
#define PING_PAYLOAD  56    // bytes, like real ping's default
#define PIT_MS_PER_TICK 10  // 1000 / pit_freq(100)

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
    printf("PING %s (%s): %d data bytes", argv[1], s, PING_PAYLOAD);

    int sent = 0, received = 0;
    uint32_t min_ms = 0xFFFFFFFF, max_ms = 0, sum_ms = 0;

    for (int i = 0; i < PING_COUNT; i++) {
        sent++;
        uint64_t t0 = pit_ticks;
        int r = icmp_ping(ip, 0x4242, (uint16)i, 500000);
        uint64_t t1 = pit_ticks;
        uint32_t elapsed_ms = (uint32_t)(t1 - t0) * PIT_MS_PER_TICK;

        if (r == NET_OK) {
            received++;
            if (elapsed_ms < min_ms) min_ms = elapsed_ms;
            if (elapsed_ms > max_ms) max_ms = elapsed_ms;
            sum_ms += elapsed_ms;

            printf("%d bytes from %s: icmp_seq=%d ttl=64 time=%d ms",
                   PING_PAYLOAD, s, i, elapsed_ms);
        } else {
            printf("Request timeout for icmp_seq %d", i);
        }
    }

    printf("\n--- %s ping statistics ---", argv[1]);
    int loss_pct = sent ? ((sent - received) * 100) / sent : 0;
    printf("%d packets transmitted, %d packets received, %d%% packet loss",
           sent, received, loss_pct);

    if (received > 0) {
        uint32_t avg_ms = sum_ms / received;
        printf("round-trip min/avg/max = %d/%d/%d ms",
               min_ms, avg_ms, max_ms);
    }

    return received > 0 ? 0 : 1;
}
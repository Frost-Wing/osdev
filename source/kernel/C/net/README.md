# FrostWing networking stack

The stack is layered like a small Linux-style path while preserving the existing RTL8139 driver as the bottom-most hardware layer:

```text
shell commands (ping, wget)
HTTP/DNS/DHCP
TCP/UDP/ICMP
IPv4 routing, checksum, TTL, fragmentation guard
ARP cache
Ethernet II
netif wrapper
RTL8139 driver registers/DMA/IRQ or polling
```

## RTL8139 interface

Upper layers must not touch RTL8139 registers. `netif_send()` and `netif_poll()` are the only hardware-facing entry points. The current integration uses the driver's polling receive path (`rtl8139_receive_packet`) from `netif_poll()`. The IRQ handler may later enqueue frames and invoke the same Ethernet receive callback.

## Packet buffers

`net_packet_t` reserves headroom so layers can prepend headers without ad-hoc pointer arithmetic. It supports append/prepend/pull operations and bounds-checks all packet growth.

## Implemented modules

* `net_core.c` - byte order, checksums, packet buffers, and netif wrapper.
* `ethernet.c` - Ethernet II parse/build and EtherType dispatch.
* `arp_ipv4_icmp_udp.c` - ARP cache, IPv4, ICMP echo, UDP queues.
* `dns_tcp_http_dhcp.c` - DNS over UDP, TCP/HTTP scaffolding, DHCP placeholder.

The Linux x86-64 syscall adapter exposes the same IPv4 stack to userland for
ICMP echo, UDP datagrams, and connected TCP streams. The default root filesystem
includes `/etc/resolv.conf` for the QEMU user-networking DNS address (`10.0.2.3`),
matching the kernel's default network configuration.

## Current limitations

TCP implements the basic SYN/SYN-ACK handshake, ordered receive buffering, ACKs,
and FIN handling used by HTTP/1.x downloads. Retransmission, congestion control,
fragment reassembly, DHCP, and HTTPS/TLS are not implemented. `wget` supports
`http://` URLs only.

## QEMU validation assumptions

For QEMU user networking with RTL8139, boot with a device like:

```sh
qemu-system-x86_64 -netdev user,id=n0 -device rtl8139,netdev=n0 ...
```

The default static configuration is `10.0.2.15/24`, gateway `10.0.2.2`, DNS `10.0.2.3`. Test in the shell with:

```sh
ping 10.0.2.2
ping example.com
wget http://example.com/ /tmp/example.html
```

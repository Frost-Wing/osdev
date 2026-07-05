#include <net/net.h>
#include <memory.h>
#define ETH_ARP 0x0806
#define ETH_IP  0x0800
struct eth_hdr{uint8 dst[6];uint8 src[6];uint16 type;} __attribute__((packed));
void ethernet_input(const uint8* frame,size_t len){ if(!frame||len<sizeof(struct eth_hdr)) return; const struct eth_hdr*h=(const struct eth_hdr*)frame; uint16 t=net_ntohs(h->type); const uint8*p=frame+sizeof(*h); size_t plen=len-sizeof(*h); if(t==ETH_ARP) arp_input(p,plen); else if(t==ETH_IP) ipv4_input(p,plen); }
int ethernet_send(uint16 ethertype,const uint8 dst[6],const void* payload,size_t len){ if(!dst||!payload||len>NET_MTU) return NET_EINVAL; net_packet_t pkt; struct eth_hdr*h; net_packet_init(&pkt); net_packet_append(&pkt,payload,len); net_packet_prepend(&pkt,sizeof(*h),(void**)&h); memcpy(h->dst,dst,6); memcpy(h->src,net_cfg.mac,6); h->type=net_htons(ethertype); return netif_send(pkt.data,pkt.len); }

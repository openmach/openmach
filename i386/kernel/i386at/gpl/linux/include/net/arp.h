/* linux/net/inet/arp.h */
#ifndef _ARP_H
#define _ARP_H

extern void	arp_init(void);
extern int	arp_rcv(struct sk_buff *skb, struct device *dev,
			struct packet_type *pt);
extern int	arp_query(unsigned char *haddr, u32 paddr, struct device *dev);
extern int	arp_find(unsigned char *haddr, u32 paddr,
		struct device *dev, u32 saddr, struct sk_buff *skb);
extern int	arp_ioctl(unsigned int cmd, void *arg);
extern void     arp_send(int type, int ptype, u32 dest_ip, 
			 struct device *dev, u32 src_ip, 
			 unsigned char *dest_hw, unsigned char *src_hw, unsigned char *th);
extern int	arp_bind_cache(struct hh_cache ** hhp, struct device *dev, unsigned short type, __u32 daddr);
extern int	arp_update_cache(struct hh_cache * hh);
#endif	/* _ARP_H */

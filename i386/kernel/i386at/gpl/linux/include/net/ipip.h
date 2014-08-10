extern int ipip_rcv(struct sk_buff *skb, struct device *dev, struct options *opt, 
		__u32 daddr, unsigned short len, __u32 saddr,
                                   int redo, struct inet_protocol *protocol);
                                   

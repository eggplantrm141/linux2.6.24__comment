#ifndef _NF_CONNTRACK_IPV6_H
#define _NF_CONNTRACK_IPV6_H

extern struct nf_conntrack_l3proto nf_conntrack_l3proto_ipv6;

extern struct nf_conntrack_l4proto nf_conntrack_l4proto_tcp6;
extern struct nf_conntrack_l4proto nf_conntrack_l4proto_udp6;
extern struct nf_conntrack_l4proto nf_conntrack_l4proto_icmpv6;

extern int nf_ct_frag6_init(void);
extern void nf_ct_frag6_cleanup(void);
extern struct sk_buff *nf_ct_frag6_gather(struct sk_buff *skb);
extern void nf_ct_frag6_output(unsigned int hooknum, struct sk_buff *skb,
			       struct net_device *in,
			       struct net_device *out,
			       int (*okfn)(struct sk_buff *));

struct inet_frags_ctl;
extern struct inet_frags_ctl nf_frags_ctl;

#endif /* _NF_CONNTRACK_IPV6_H*/

#
# Makefile for the netfilter modules on top of IPv4.
#

# objects for l3 independent conntrack
nf_conntrack_ipv4-objs  :=  nf_conntrack_l3proto_ipv4.o nf_conntrack_proto_icmp.o
ifeq ($(CONFIG_NF_CONNTRACK_PROC_COMPAT),y)
ifeq ($(CONFIG_PROC_FS),y)
nf_conntrack_ipv4-objs	+= nf_conntrack_l3proto_ipv4_compat.o
endif
endif

nf_nat-objs		:= nf_nat_core.o nf_nat_helper.o nf_nat_proto_unknown.o nf_nat_proto_tcp.o nf_nat_proto_udp.o nf_nat_proto_icmp.o
iptable_nat-objs	:= nf_nat_rule.o nf_nat_standalone.o

# connection tracking
obj-$(CONFIG_NF_CONNTRACK_IPV4) += nf_conntrack_ipv4.o

obj-$(CONFIG_NF_NAT) += nf_nat.o

# NAT helpers (nf_conntrack)
obj-$(CONFIG_NF_NAT_AMANDA) += nf_nat_amanda.o
obj-$(CONFIG_NF_NAT_FTP) += nf_nat_ftp.o
obj-$(CONFIG_NF_NAT_H323) += nf_nat_h323.o
obj-$(CONFIG_NF_NAT_IRC) += nf_nat_irc.o
obj-$(CONFIG_NF_NAT_PPTP) += nf_nat_pptp.o
obj-$(CONFIG_NF_NAT_SIP) += nf_nat_sip.o
obj-$(CONFIG_NF_NAT_SNMP_BASIC) += nf_nat_snmp_basic.o
obj-$(CONFIG_NF_NAT_TFTP) += nf_nat_tftp.o

# NAT protocols (nf_nat)
obj-$(CONFIG_NF_NAT_PROTO_GRE) += nf_nat_proto_gre.o

# generic IP tables 
obj-$(CONFIG_IP_NF_IPTABLES) += ip_tables.o

# the three instances of ip_tables
obj-$(CONFIG_IP_NF_FILTER) += iptable_filter.o
obj-$(CONFIG_IP_NF_MANGLE) += iptable_mangle.o
obj-$(CONFIG_NF_NAT) += iptable_nat.o
obj-$(CONFIG_IP_NF_RAW) += iptable_raw.o

# matches
obj-$(CONFIG_IP_NF_MATCH_ADDRTYPE) += ipt_addrtype.o
obj-$(CONFIG_IP_NF_MATCH_AH) += ipt_ah.o
obj-$(CONFIG_IP_NF_MATCH_ECN) += ipt_ecn.o
obj-$(CONFIG_IP_NF_MATCH_IPRANGE) += ipt_iprange.o
obj-$(CONFIG_IP_NF_MATCH_OWNER) += ipt_owner.o
obj-$(CONFIG_IP_NF_MATCH_RECENT) += ipt_recent.o
obj-$(CONFIG_IP_NF_MATCH_TOS) += ipt_tos.o
obj-$(CONFIG_IP_NF_MATCH_TTL) += ipt_ttl.o

# targets
obj-$(CONFIG_IP_NF_TARGET_CLUSTERIP) += ipt_CLUSTERIP.o
obj-$(CONFIG_IP_NF_TARGET_ECN) += ipt_ECN.o
obj-$(CONFIG_IP_NF_TARGET_LOG) += ipt_LOG.o
obj-$(CONFIG_IP_NF_TARGET_MASQUERADE) += ipt_MASQUERADE.o
obj-$(CONFIG_IP_NF_TARGET_NETMAP) += ipt_NETMAP.o
obj-$(CONFIG_IP_NF_TARGET_REDIRECT) += ipt_REDIRECT.o
obj-$(CONFIG_IP_NF_TARGET_REJECT) += ipt_REJECT.o
obj-$(CONFIG_IP_NF_TARGET_SAME) += ipt_SAME.o
obj-$(CONFIG_IP_NF_TARGET_TOS) += ipt_TOS.o
obj-$(CONFIG_IP_NF_TARGET_TTL) += ipt_TTL.o
obj-$(CONFIG_IP_NF_TARGET_ULOG) += ipt_ULOG.o

# generic ARP tables
obj-$(CONFIG_IP_NF_ARPTABLES) += arp_tables.o
obj-$(CONFIG_IP_NF_ARP_MANGLE) += arpt_mangle.o

# just filtering instance of ARP tables for now
obj-$(CONFIG_IP_NF_ARPFILTER) += arptable_filter.o

obj-$(CONFIG_IP_NF_QUEUE) += ip_queue.o


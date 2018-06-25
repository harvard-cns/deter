#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/tcp.h>

static unsigned int dropper_fn(void *priv, struct sk_buff *skb, const struct nf_hook_state *state){
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = (struct tcphdr *)((u32 *)iph + iph->ihl);
	if (tcph->dest == 0x60ea){
		if (!(ntohs(iph->id) & 0xff))
			return NF_DROP;
	}
	return NF_ACCEPT;
}

static struct nf_hook_ops dropper_nf_hook_ops = {
	.hook = dropper_fn,
	.pf = NFPROTO_IPV4,
	.hooknum = NF_INET_LOCAL_IN,
	.priority = NF_IP_PRI_FIRST,
};

static int __init dropper_init(void)
{
	if (nf_register_hook(&dropper_nf_hook_ops))
		return -1;
	return 0;
}

static void __exit dropper_exit(void)
{
	nf_unregister_hook(&dropper_nf_hook_ops);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yuliang Li");
MODULE_DESCRIPTION("dropper");

module_init(dropper_init);
module_exit(dropper_exit);

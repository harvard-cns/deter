#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/tcp.h>

int bucket_sport[65536];
int bucket_dport[65536];

static inline int random(void){
	int res;
	get_random_bytes(&res, sizeof(res));
	return res;
}

static unsigned int dropper_fn(void *priv, struct sk_buff *skb, const struct nf_hook_state *state){
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = (struct tcphdr *)((u32 *)iph + iph->ihl);
	if (tcph->dest == 0x5ac3 || tcph->dest == 0x63ea || tcph->dest == 0x62ea || tcph->dest == 0x61ea){ // htons(50010), htons(60003), htons(60002), htons(60001)
		if (bucket_sport[tcph->source] > 0){
			printk("drop the packet after 0-win, sport:%hu\n", ntohs(tcph->source));
			bucket_sport[tcph->source]--;
			if (bucket_sport[tcph->source] == 0)
				bucket_sport[tcph->source] = -1; // no more drops for this connection
			return NF_DROP;
		}
		// if this is a zero window packet, drop the next packet (probably window update)
		if (tcph->window == 0){
			if (bucket_sport[tcph->source] == 0 && (random() & 0x7) == 0){// drop with probability
				bucket_sport[tcph->source] = 3;
				printk("a Zero window\n");
			}
		}
	}else if (tcph->source == 0x5ac3 || tcph->source == 0x63ea || tcph->source == 0x62ea || tcph->source == 0x61ea){ // htons(50010), htons(60003), htons(60002), htons(60001)
		if (bucket_dport[tcph->dest] > 0){
			printk("drop the packet after 0-win, dport:%hu\n", ntohs(tcph->dest));
			bucket_dport[tcph->dest]--;
			if (bucket_dport[tcph->dest] == 0)
				bucket_dport[tcph->dest] = -1; // no more drops for this connection
			return NF_DROP;
		}
		// if this is a zero window packet, drop the next packet (probably window update)
		if (tcph->window == 0){
			if (bucket_dport[tcph->dest] == 0 && (random() & 0x7) == 0){// drop with probability
				bucket_dport[tcph->dest] = 3;
				printk("a Zero window\n");
			}
		}
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
	memset(bucket_sport, 0, sizeof(bucket_sport));
	memset(bucket_dport, 0, sizeof(bucket_dport));
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

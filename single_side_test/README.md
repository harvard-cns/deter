These are the scripts for testing single side replay.
It requires (1) add iptables rule and (2) tcpreplay

We need iptables rules because we need to prevent the incoming packets (derand replayed) from being handled by linux kernel and thus generate rst packets. So we need to drop incoming derand replayed packets.

Then we use tcpreplay to regenerate packets from this side.

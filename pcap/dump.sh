if [ $# -ne 1 ]; then
	echo "usage: bash dump.sh <pcap_name>"
else
	sudo tcpdump -s 96 -i ens8 -w $1 -B 10000000 port 60000
fi

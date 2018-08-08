if [ $# -ne 1 ]; then
	echo "usage: bash dump.sh <pcap_name>"
else
	sudo tcpdump -s 96 -i ens8 -w $1 -B 10000000 portrange 60000-60003 or port 50010
fi

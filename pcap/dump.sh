if [ $# -eq 0 ]; then
	echo "usage: bash dump.sh <pcap_name> [<tcpdump_options>...]"
else
	file=$1
	shift
	if [ $# -gt 0 ]; then
		sudo tcpdump -s 96 -i ens3 -w $file -B 10000000 "(portrange 60000-60003 or port 50010) and ($*)"
	else
		sudo tcpdump -s 96 -i ens3 -w $file -B 10000000 "(portrange 60000-60003 or port 50010)"
	fi
fi

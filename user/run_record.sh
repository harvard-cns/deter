source ip_util.sh

if [ $# -ge 2 ]; then
	echo "usage: bash run_record.sh [<dstip>]"
	echo "       <dstip>: specific the connection to record."
else
	if [ $# -eq 1 ]; then
		dstip=$1
	else
		dstip=0.0.0.0
	fi
	dstip_int=$(ip_to_int $dstip)
	cd ../pcap
	bash dump.sh runtime.pcap "(host $dstip)" &

	sleep 1

	cd ../kmod
	sudo insmod derand_recorder.ko dstip=$dstip_int

	cd ../user
	sudo ./recorder
fi

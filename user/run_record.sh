if [ $# -ge 2 ]; then
	echo "usage: bash run_record.sh [<dstip(hex)>]"
	echo "       <dstip(hex)>: specific the connection to record. Should be in hex"
else
	if [ $# -eq 1 ]; then
		dstip=$1
	else
		dstip=0
	fi
	cd ../pcap
	bash dump.sh runtime.pcap &

	sleep 1

	cd ../kmod
	sudo insmod derand_recorder.ko dstip=$dstip

	cd ../user
	sudo ./recorder
fi

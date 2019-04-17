if [ $# -le 0 ]; then
	echo "usage: bash run_replay.sh <record> [<dstip>]"
else
	cd ../pcap
	bash dump.sh replay.pcap &

	sleep 1

	cd ../kmod
	sudo insmod deter_replayer.ko

	cd ../user
	sudo ./logger > log.txt &
	sudo ./replay $1 $2
fi

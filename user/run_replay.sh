if [ $# -le 0 ]; then
	echo "usage: bash run_replay.sh <record> [<dstip>]"
else
	cd ../pcap
	bash dump.sh &

	cd ../kmod
	sudo insmod derand_replayer.ko

	cd ../user
	sudo ./logger > log.txt &
	sudo ./replay $1 $2
fi

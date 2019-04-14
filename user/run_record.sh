source ip_util.sh

dstip=0.0.0.0
ndstip=0.0.0.0
do_tcpdump=0
n_cpu=1
while [[ $# -gt 0 ]]
do
	key=$1
	case $key in 
	-h|--help)
		echo "usage: bash run_record.sh [OPTION]..."
		echo "options:"
		echo "-d, --dstip             specify the dstip of the connection to record"
		echo "-n, --ndstip            specify the dstip NOT to record"
		echo "-p, --tcpdump           do tcpdump"
		echo "-c, --cpu               number of cpu"
		shift
		exit 0
	;;
	-d|--dstip)
		dstip=$2
		shift
		shift
	;;
	-n|--ndstip)
		ndstip=$2
		shift
		shift
	;;
	-p|--tcpdump)
		do_tcpdump=1
		shift
	;;
	-c|--cpu)
		n_cpu=$2
		shift
		shift
	;;
	*)
		echo "unknown argument:" $key
		shift
	esac
done

dstip_int=$(ip_to_int $dstip)
ndstip_int=$(ip_to_int $ndstip)

if [ $do_tcpdump -eq 1 ]; then
	cd ../pcap
	bash dump.sh runtime.pcap "(host $dstip)" &
	sleep 1.5
fi

cd ../kmod
sudo insmod derand_recorder.ko dstip=$dstip_int ndstip=$ndstip_int

cd ../user
sudo ./recorder

cd ../pcap
bash dump.sh runtime.pcap &

sleep 1

cd ../kmod
sudo insmod derand_recorder.ko

cd ../user
sudo ./recorder

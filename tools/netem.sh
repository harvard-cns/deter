usage(){
	echo "usage: bash netem.sh <add/del>"
}
run(){
	echo "$@"
	eval "$@"
}

if [ $# -le 0 ]; then 
	usage
else
	if [ $1 = "add" ]; then
		run sudo tc qdisc add dev ens8 root netem loss 30% 50%
	elif [ $1 = "del" ]; then 
		run sudo tc qdisc del dev ens8 root
	else
		usage
	fi
fi

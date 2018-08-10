function ip_to_int
{
	#Returns the integer representation of an IP arg, passed in ascii dotted-decimal notation (x.x.x.x)
	IP=$1
	IPNUM=0
	for (( i=0 ; i<4 ; ++i )); do
		((IPNUM+=${IP%%.*}*$((256**$((3-${i}))))))
		IP=${IP#*.}
	done
	echo $IPNUM
} 

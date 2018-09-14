import glob
import subprocess
import re
if __name__ == "__main__":
	records = glob.glob("records_spark/0a00000*")
	total_bytes = 0
	total_mstamp = 0
	total_evts = 0
	total_evts_dynamic = 0;
	total_evts_huffman_prefix = 0;
	total_evts_huffman_encoding = 0;
	total_sockcall = 0
	total_jiffies = 0
	total_tcpdump = 0
	total_tx_stamp = 0
	total_tx_stamp_sample_dynamic = 0;
	total_tx_stamp_sample_huffman_prefix = 0;
	total_loss = 0;
	total_transfer = 0;
	for rec in records:
		print rec
		m = re.search("/(.*):.*->(.*):", rec)
		srcip = m.group(1)
		dstip = m.group(2)
		if srcip == dstip:
			continue
		print srcip, dstip
		cmd = "./reader '%s' get_meta | grep 'broken' | cut -d' ' -f2"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		if out != '0':
			print "broken %s, skip"%out
			#continue

		cmd = "./reader '%s' get_meta | grep 'compressed total' | cut -d' ' -f3"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_bytes += int(out)
		print "total:", out

		cmd = "./reader '%s' get_meta | grep '^mstamp' | cut -d' ' -f7"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_mstamp += int(out)
		print "mstamp:", out

		cmd = "./reader '%s' get_meta | grep '^evts:' | cut -d' ' -f2"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_evts += int(out)
		print "evts:", out

		cmd = "./reader '%s' get_meta | grep '^\tevts: dynamic:' | cut -d' ' -f3"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_evts_dynamic += int(out)
		print "evts, dynamic:", out

		cmd = "./reader '%s' get_meta | grep '^\tevts: huffman_prefix:' | cut -d' ' -f3"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_evts_huffman_prefix += int(out)
		print "evts, huffman_prefix:", out

		cmd = "./reader '%s' get_meta | grep '^\tevts: huffman_encoding:' | cut -d' ' -f3"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_evts_huffman_encoding += int(out)
		print "evts, huffman_encoding:", out

		cmd = "./reader '%s' get_meta | grep '^sockcalls:' | cut -d' ' -f2"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_sockcall += int(out)
		print "sockcalls:", out

		cmd = "./reader '%s' get_meta | grep '^jiffies:' | cut -d' ' -f2"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_jiffies += int(out)

		#cmd = "./reader '%s' | grep 'tcp_transmit_skb\[0' | wc -l"%rec
		cmd = "./reader '%s' get_meta | grep 'packets received' | cut -d' ' -f3"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_tcpdump += int(out)
		print "packets:", out

		cmd = "./reader '%s' get_meta | grep 'tx_stamp size, huffman_prefix' | cut -d' ' -f4"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_tx_stamp += int(out)
		print "tx_stamp, huffman_prefix:", out

		cmd = "./reader '%s' get_meta | grep 'tx_stamp size, sample, dynamic' | cut -d' ' -f8"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_tx_stamp_sample_dynamic += int(out)
		print "tx_stamp, sample, dynamic:", out

		cmd = "./reader '%s' get_meta | grep 'tx_stamp size, sample, huffman_prefix' | cut -d' ' -f8"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_tx_stamp_sample_huffman_prefix += int(out)
		print "tx_stamp, sample, huffman_prefix:", out

		cmd = "./reader '%s' | grep 'drops' | cut -d' ' -f1"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_loss += int(out)
		print "loss:", out

		cmd = "./reader '%s' | grep 'fin_seq' | cut -d' ' -f2"%rec
		out = subprocess.check_output(cmd, shell=True).strip()
		total_transfer += int(out)
		print "transfer (Bytes):", out
	print total_bytes, total_mstamp, total_tcpdump, total_evts, total_sockcall, total_jiffies
	print 'ts_stamp, no sample:', total_tx_stamp, 'sample_dynamic:', total_tx_stamp_sample_dynamic, 'sample_huffman_prefix:', total_tx_stamp_sample_huffman_prefix
	print 'evts, dynamic:', total_evts_dynamic, 'huffman_prefix:', total_evts_huffman_prefix, 'huffman_encoding:', total_evts_huffman_encoding
	print 'total_loss:', total_loss
	print 'total_transfer:', total_transfer

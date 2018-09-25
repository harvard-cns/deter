import glob
import subprocess
import re
if __name__ == "__main__":
	records = glob.glob("/usr/hadoop/log/*")
	records = glob.glob("log/*")
	total_bytes = 0
	total_mstamp = 0
	total_evts = 0
	total_evts_dynamic = 0;
	total_evts_huffman_prefix = 0;
	total_evts_huffman_encoding = 0;
	total_sockcall = 0
	total_jiffies = 0
	total_pkt_rx = 0
	total_pkt_tx = 0
	total_tx_stamp = 0
	total_tx_stamp_sample_dynamic = 0;
	total_tx_stamp_sample_huffman_prefix = 0;
	total_loss = 0;
	total_transfer = 0;
	for rec in records:
		print rec
		m = re.search("/([^/]*):.*->(.*):", rec)
		srcip = m.group(1)
		dstip = m.group(2)
		if srcip == dstip:
			continue
		print srcip, dstip

		# put result in a temp file
		cmd = "./reader '%s' > tmp_result.txt"%rec
		subprocess.call(cmd, shell=True)

		cmd = "cat tmp_result.txt | grep 'broken' | cut -d' ' -f2"
		out = subprocess.check_output(cmd, shell=True).strip()
		if out != '0':
			print "broken %s, skip"%out
			#continue

		cmd = "cat tmp_result.txt | grep 'compressed total' | cut -d' ' -f3"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_bytes += int(out)
		print "total:", out

		cmd = "cat tmp_result.txt | grep '^mstamp' | cut -d' ' -f7"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_mstamp += int(out)
		print "mstamp:", out

		cmd = "cat tmp_result.txt | grep '^evts:' | cut -d' ' -f2"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_evts += int(out)
		print "evts:", out

		cmd = "cat tmp_result.txt | grep '^\tevts: dynamic:' | cut -d' ' -f3"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_evts_dynamic += int(out)
		print "evts, dynamic:", out

		cmd = "cat tmp_result.txt | grep '^\tevts: huffman_prefix:' | cut -d' ' -f3"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_evts_huffman_prefix += int(out)
		print "evts, huffman_prefix:", out

		cmd = "cat tmp_result.txt | grep '^\tevts: huffman_encoding:' | cut -d' ' -f3"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_evts_huffman_encoding += int(out)
		print "evts, huffman_encoding:", out

		cmd = "cat tmp_result.txt | grep '^sockcalls:' | cut -d' ' -f2"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_sockcall += int(out)
		print "sockcalls:", out

		cmd = "cat tmp_result.txt | grep '^jiffies:' | cut -d' ' -f2"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_jiffies += int(out)

		cmd = "cat tmp_result.txt | grep 'packets received' | cut -d' ' -f3"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_pkt_rx += int(out)
		print "packets(rx):", out, total_pkt_rx

		cmd = "cat tmp_result.txt | grep 'tsq' | cut -d' ' -f1"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_pkt_tx += int(out)
		print "packets(tx):", out, total_pkt_tx

		cmd = "cat tmp_result.txt | grep 'tx_stamp size, huffman_prefix' | cut -d' ' -f4"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_tx_stamp += int(out)
		print "tx_stamp, huffman_prefix:", out

		cmd = "cat tmp_result.txt | grep 'tx_stamp size, sample, dynamic' | cut -d' ' -f8"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_tx_stamp_sample_dynamic += int(out)
		print "tx_stamp, sample, dynamic:", out

		cmd = "cat tmp_result.txt | grep 'tx_stamp size, sample, huffman_prefix' | cut -d' ' -f8"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_tx_stamp_sample_huffman_prefix += int(out)
		print "tx_stamp, sample, huffman_prefix:", out

		cmd = "cat tmp_result.txt | grep 'drops' | cut -d' ' -f1"
		out = subprocess.check_output(cmd, shell=True).strip()
		total_loss += int(out)
		print "loss:", out

		cmd = "cat tmp_result.txt | grep 'fin_seq' | cut -d' ' -f2"
		out = subprocess.check_output(cmd, shell=True).strip()
		if (int(out) == 0):
			cmd = "cat tmp_result.txt | grep 'total bytes sent' | cut -d' ' -f4"
			out = subprocess.check_output(cmd, shell=True).strip()
		total_transfer += int(out)
		print "transfer (Bytes):", out
	print total_bytes, total_mstamp, total_pkt_rx, total_pkt_tx, total_evts, total_sockcall, total_jiffies
	print 'ts_stamp, no sample:', total_tx_stamp, 'sample_dynamic:', total_tx_stamp_sample_dynamic, 'sample_huffman_prefix:', total_tx_stamp_sample_huffman_prefix
	print 'evts, dynamic:', total_evts_dynamic, 'huffman_prefix:', total_evts_huffman_prefix, 'huffman_encoding:', total_evts_huffman_encoding
	print 'total_loss:', total_loss
	print 'total_transfer:', total_transfer

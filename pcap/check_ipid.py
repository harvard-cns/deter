with open("tmp.txt","r") as fin:
	lines = map(lambda x: int(x.strip(',\n')), fin.readlines())
last = lines[0]
for i in lines[1:]:
	gap = (i - last + 65536) % 65536
	if gap > 100:
		print i, last
	last = i

import sys
import os


def usage():
    print ("Parse fct time")
    print ("python3 fct.py <input file name> <waste file> <output file name>")
    exit(1)

def do(inf, outf):
	inf.readline()
	lines = inf.readlines()

	ss_total = 0
	ss_time = 0
	s_total = 0
	s_time = 0
	m_total = 0
	m_time = 0
	l_total = 0
	l_time = 0
	xl_total = 0
	xl_time = 0
	

	for line in lines:
		tmp = line.strip().split(",")

		print (tmp[0]+"/"+tmp[1]+"/"+tmp[2]+"+\n")
		size = float(tmp[1].strip())
		fct = float(tmp[2].strip())

		if(0<size and size <=1000):
			ss_total +=1
			ss_time +=fct	
		if(0<size and size <=10000):
			s_total +=1
			s_time +=fct	
		elif(10000<size and size <=100000):
			m_total+=1
			m_time +=fct	
		elif(100000<size and size <=1000000):
			l_total+=1
			l_time +=fct	
		elif(1000000<size):
			xl_total+=1
			xl_time +=fct


	if ss_total != 0:
		 outf.write("ss_total "+str(ss_total)+", ss_avg_time "+str(ss_time/ss_total) +"\n")	
	if s_total != 0:
		outf.write("s_total "+str(s_total)+", s_avg_time "+str(s_time/s_total) +"\n")	
	if m_total != 0:
		outf.write("m_total "+str(m_total)+", m_avg_time "+str(m_time/m_total) +"\n")	
	if l_total != 0:
		outf.write("l_total "+str(l_total)+", l_avg_time "+str(l_time/l_total) +"\n")	
	if xl_total != 0:
		outf.write("xl_total "+str(xl_total)+", xl_avg_time "+str(xl_time/xl_total) +"\n")	
	if ss_total != 0:
		outf.write("\n"+str(ss_time/ss_total) +"\n")	
	if s_total != 0:
		outf.write(str(s_time/s_total) +"\n")	
	if m_total != 0:
		outf.write(str(m_time/m_total) +"\n")	
	if l_total != 0:
		outf.write(str(l_time/l_total) +"\n")	
	if xl_total != 0:
		outf.write(str(xl_time/xl_total) +"\n")	








def main():
	if len(sys.argv) !=3:
		usage()

	inf = open(sys.argv[1], "r")
	outf = open(sys.argv[2], "w")
	
	do(inf, outf)

	inf.close()
	outf.close()

if __name__ == "__main__":
	main()

infile = open("prerequs.txt")
outfile = open("install_prerequs.sh", "w")
L = len(infile.readlines())
infile.seek(0)
s = infile.readline()
s = infile.readline()

def checkline():
    s = infile.readline()
    s = "".join(list(s)[:-1])
    b = s.split(" ")
    c = [ele for ele in b if ele!=""]
    line = "python -m pip install %s=='%s'\n" % (c[0], c[1])
    outfile.write(line)
for i in range(L-2):
    checkline()


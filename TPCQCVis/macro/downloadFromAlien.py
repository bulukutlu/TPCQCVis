import os
import subprocess

local_dir = "/home/berki/alice/data/2022/LHC22s/"
directory = "/alice/data/2022/LHC22s/"
runList = ["529397","529399","529403","529414","529418"]
apass = "apass4"

for run in runList:
    target = subprocess.run(["alien.py","find",directory+run+"/"+apass+"/QC/001/","QC.root"], capture_output=True)
    if len(target.stdout) > 0 : 
        target_path = target.stdout[:-1].decode('UTF-8')
        print("Downloading "+target_path)
        subprocess.run(["alien.py","cp","alien:"+target_path,"file:"+local_dir+run+".root"])
    else:
        target = subprocess.run(["alien.py","find",directory+run+"/"+apass+"/","QC_fullrun.root"], capture_output=True)
        if len(target.stdout) > 0 : 
            target_path = target.stdout[:-1].decode('UTF-8')
            print("Downloading "+target_path)
            subprocess.run(["alien.py","cp","alien:"+target_path,"file:"+local_dir+run+".root"])
        else: 
            print("File "+directory+run+"/"+apass+"/QC/001/QC.root"+ "  not found!")


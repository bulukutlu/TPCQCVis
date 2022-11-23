import os
import subprocess
import ROOT
ROOT.gROOT.LoadMacro("/home/berki/Software/TPCQCVis/TPCQCVis/macro/plotQCData.C+")
ROOT.gROOT.SetBatch(True)

local_dir = "/home/berki/alice/data/2022/LHC22s/"
os.system("cd "+local_dir)
runList = ["529397","529399","529403","529414","529418"]

for run in runList:
    test = ROOT.plotQCData(local_dir+run+".root")
    #target = subprocess.run(["root","-q","-b","+\'(\""+run+".root\")\'"], capture_output=False)
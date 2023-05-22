import os
import subprocess
import ROOT
import glob
from array import array

ROOT.gROOT.LoadMacro("/home/berki/Software/TPCQCVis/TPCQCVis/macro/plotQCData.C+")
ROOT.gROOT.SetBatch(True)

local_dir = "/cave/alice/data/2022/LHC22o/test/"
os.system("cd "+local_dir)

addRunParam = True
# if it gets stuck or progresses very slowly when downloading stuff from CCDB, try accessing a different instance of the CCDB via one of:
# export alien_CLOSE_SE=ALICE::UPB::EOS
# export alien_CLOSE_SE=ALICE::GSI::SE2
# export alien_CLOSE_SE=ALICE::FZK::SE

fileList = glob.glob(local_dir+"*.root")
runList = [file[-11:-5] for file in fileList]

#runList = ["529066","529067","529077","529078","529084","529088","529115","529116","529117","529128","529129","529208","529209","529210","529211","529235","529237","529242","529248","529252","529270","529306","529310","529317","529320","529324","529337","529338","529341"]

for run in runList:
    path = local_dir+run+".root"
    if os.path.isfile(path):
        print("Plotting run", run)
        test = ROOT.plotQCData(path)
    if addRunParam:
        command = 'o2-calibration-get-run-parameters -r '+run
        print("Adding parameters for run", command)
        os.system(command)
        with open('IR.txt') as f:
            lines = f.readlines()
        IR  = array('d',[float(lines[0])])
        with open('Duration.txt') as f:
            lines = f.readlines()
        Duration = array('d',[float(lines[0])])
        with open('BField.txt') as f:
            lines = f.readlines()
        BField = array('d',[float(lines[0])])
        output = ROOT.TFile(local_dir+run+"_QC.root","update")
        tree = ROOT.TTree("RunParameters","RunParameters")
        tree.Branch("IR",  IR,  'IR/D')
        tree.Branch("Duration",  Duration,  'Duration/D')
        tree.Branch("BField",  BField,  'BField/D')
        tree.Fill()
        output.Write()
        os.remove("IR.txt")
        os.remove("Duration.txt")
        os.remove("BField.txt")

    #target = subprocess.run(["root","-q","-b","+\'(\""+run+".root\")\'"], capture_output=False)
import os
import subprocess
import ROOT
import glob
from array import array
import argparse

ROOT.gROOT.LoadMacro("/home/berki/Software/TPCQCVis/TPCQCVis/macro/plotQCData.C+")
ROOT.gROOT.SetBatch(True)

def main(local_dir, add_run_param, rerun, target):
    if target:
        if os.path.isfile(target):
                print("Plotting target", target)
                test = ROOT.plotQCData(target)
    
    else:
        os.system("cd " + local_dir)
    
        fileList_processed = glob.glob(local_dir + "*_QC.root")
        runList_processed = [file[-14:-8] for file in fileList_processed]

        fileList = glob.glob(local_dir + "*.root")
        fileList = [file for file in fileList if file not in fileList_processed]
        runList = [file[file.rfind('/')+1:-5] for file in fileList]

        if not rerun:
            runList = [run for run in runList if run not in runList_processed]
        for run in runList:
            path = local_dir + run + ".root"
            print("Path:", path)
            if os.path.isfile(path):
                print("Plotting run", run)
                test = ROOT.plotQCData(path)
            if add_run_param:
                command = 'o2-calibration-get-run-parameters -r ' + run
                print("Adding parameters for run", command)
                os.system(command)
                with open('IR.txt') as f:
                    lines = f.readlines()
                IR = array('d', [float(lines[0])])
                with open('Duration.txt') as f:
                    lines = f.readlines()
                Duration = array('d', [float(lines[0])])
                with open('BField.txt') as f:
                    lines = f.readlines()
                BField = array('d', [float(lines[0])])
                output = ROOT.TFile(local_dir + run + "_QC.root", "update")
                tree = ROOT.TTree("RunParameters", "RunParameters")
                tree.Branch("IR", IR, 'IR/D')
                tree.Branch("Duration", Duration, 'Duration/D')
                tree.Branch("BField", BField, 'BField/D')
                tree.Fill()
                output.Write()
                os.remove("IR.txt")
                os.remove("Duration.txt")
                os.remove("BField.txt")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for processing data")
    parser.add_argument("local_dir", help="Path to the local directory")
    parser.add_argument(
        "--add_run_param",
        action="store_true",
        help="Add run parameters (default: False)"
    )
    parser.add_argument(
        "--rerun",
        action="store_true",
        help="Rerun plotter for existing files (default: False)"
    )
    parser.add_argument("--target", help="Directly plot target ROOT file")
    args = parser.parse_args()

    main(args.local_dir, args.add_run_param, args.rerun, args.target)

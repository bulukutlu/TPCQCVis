import os
import subprocess
import ROOT
import glob
from array import array
import argparse
import concurrent.futures
import TPCQCVis.tools.periodPostprocessing as periodPostprocessing

# Get the environment variables
CODEDIR = os.environ['TPCQCVIS_DIR']
DATADIR = os.environ['TPCQCVIS_DATA']
REPORTDIR = os.environ['TPCQCVIS_REPORT']

# Load the ROOT macro for plotting
ROOT.gROOT.LoadMacro(f"{CODEDIR}/TPCQCVis/macro/plotQCData.C+")
ROOT.gROOT.SetBatch(True)

def addMovingWindow(path):
    file = ROOT.TFile(path,"r")
    if not file.Get("mw"): return 0
    
    with ROOT.TFile(path.split(".root")[0]+"_QC.root", "update") as outfile:
        outfile.cd()
        for folder in file.mw.TPC.GetListOfKeys():
            timestamps = [timestamp.GetName() for timestamp in file.mw.TPC.Get(folder.GetName()).GetListOfKeys()]
            objects = list(set([obj.GetName() for obj in file.mw.TPC.Get(folder.GetName()).Get(timestamps[0])]))
            if not len(timestamps) : continue
            outfile.cd(folder.GetName()+"QC")
            for item in range(len(objects)):
                ROOT.gDirectory.mkdir(objects[item]+"_mw")
                outfile.cd(folder.GetName()+"QC/"+objects[item]+"_mw")
                for timestamp in timestamps:
                    histo = file.mw.TPC.Get(folder.GetName()).Get(timestamp).At(item).getObject()
                    histo.SetName(timestamp)
                    histo.Write("")

def plot(local_dir, path):
    plotter_command = f"python {CODEDIR}/TPCQCVis/tools/runPlotter.py {local_dir} --target {path}"
    subprocess.run(plotter_command, shell=True)

def plot_run_param(local_dir, path):
    plotter_command = f"python {CODEDIR}/TPCQCVis/tools/runPlotter.py {local_dir} --target {path} --add_run_param"
    subprocess.run(plotter_command, shell=True)

def run_param_func(local_dir, run):
    command = 'o2-calibration-get-run-parameters -r ' + run
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
    os.remove("DetList.txt")
    output.Close()

def main(local_dir, add_run_param, rerun, target, threads, period_postprocessing):
    if target:
        if os.path.isfile(target):
            if add_run_param:
                run=target[-11:-5]
                print("Adding parameters for run ", run)
                test = ROOT.plotQCData(target)
                addMovingWindow(target)
                run_param_func(local_dir, run)
            else:
                print("Plotting target", target)
                test = ROOT.plotQCData(target)
                addMovingWindow(target)
        else:
            print("Target ", target, " doesn't exist!")
    else:
        os.system("cd " + local_dir)    
        fileList = glob.glob(local_dir + "*.root")
        fileList = [file for file in fileList if ("periodOverview" not in file and "_QC" not in file)]
        runList = [file[file.rfind('/')+1:-5] for file in fileList]
        if not rerun:
            fileList_processed = glob.glob(local_dir + "*_QC.root")
            runList_processed = [file[file.rfind('/')+1:-8] for file in fileList_processed]
            if len(runList_processed) > 0:
                print(f"Not rerunning {len(runList_processed)} runs in path, use --rerun to rerun.")
            runList = [run for run in runList if run not in runList_processed]              
        print("Run list to be processed:", runList)
        
        if threads > 1:
            if add_run_param:
                with concurrent.futures.ThreadPoolExecutor(max_workers=threads) as executor:
                    futures = []
                    for run in runList:
                        path = local_dir + run + ".root"
                        if os.path.isfile(path):
                            futures.append(executor.submit(plot, local_dir, path))
                            plot_run_param(local_dir, path)
            else:
                print("Threads =", threads)
                with concurrent.futures.ThreadPoolExecutor(max_workers=threads) as executor:
                    futures = []
                    for run in runList:
                        path = local_dir + run + ".root"
                        if os.path.isfile(path):
                            futures.append(executor.submit(plot, local_dir, path))        
        else:
            if add_run_param:
                for run in runList:
                    path = local_dir + run + ".root"
                    if os.path.isfile(path):                            
                        print("Adding parameters for run", run)
                        plot_run_param(local_dir, path)
            else:
                for run in runList:
                    path = local_dir + run + ".root"
                    if os.path.isfile(path):
                        plot(local_dir, path)


        if period_postprocessing:
            print("Running period postprocessing")   
            fileListAll = glob.glob(local_dir + "*_QC.root")
            runListAll = [file[-14:-8] for file in fileListAll]             
            periodPostprocessing.main(local_dir, fileListAll, runListAll)
            


if __name__ == "__main__":
    # Get the command-line arguments
    parser = argparse.ArgumentParser(description="Script for processing data")
    parser.add_argument("local_dir", help="Path to the local directory")
    parser.add_argument(
        "--add_run_param",
        action="store_true",
        dest="add_run_param",
        default=True,
        help="Add run parameters (default: True)"
    )
    parser.add_argument(
        "--not_add_run_param",
        action="store_false",
        dest="add_run_param",
        help="Does not add run parameters (default: False)"
    )
    parser.add_argument(
        "--period_postprocessing",
        action="store_false",
        help="Add run parameters (default: True)"
    )
    parser.add_argument(
        "--rerun",
        action="store_true",
        help="Rerun plotter for existing files (default: False)"
    )
    parser.add_argument("--target", help="Directly plot target ROOT file")
    parser.add_argument("-t", "--num_threads", type=int, default=1, help="Number of threads to be used (default: 1)")
    args = parser.parse_args()
    # Execute the main function
    if not args.target:
        print("Running plotter")
    main(args.local_dir, args.add_run_param, args.rerun, args.target, args.num_threads, args.period_postprocessing)

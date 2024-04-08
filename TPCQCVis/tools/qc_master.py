import subprocess
import argparse
import concurrent.futures
import os

CODEDIR = os.environ['TPCQCVIS_DIR']
DATADIR = os.environ['TPCQCVIS_DATA']
REPORTDIR = os.environ['TPCQCVIS_REPORT']

def download(path, period, apass):
    download_command = f"python {CODEDIR}/TPCQCVis/tools/downloadFromAlien.py {path}/{period}/{apass}/ /alice/data/20{period[3:5]}/{period}/ {apass}"
    print(f"Executing download command for {path}/{period}/{apass}/")
    subprocess.run(download_command, shell=True)

def plot(path, period, apass, rerun):
    if os.path.isdir(f"{path}/{period}/{apass}/"):
        plotter_command = f"python {CODEDIR}/TPCQCVis/tools/runPlotter.py {path}/{period}/{apass}/"
        if rerun:
            plotter_command += " --rerun"
        print(f"Executing plotter command for {path}/{period}/{apass}/")
        subprocess.run(plotter_command, shell=True)

def generate_report(path, period, apass):
    if os.path.isdir(f"{path}/{period}/{apass}/"):
        report_command = f"python {CODEDIR}/TPCQCVis/tools/generateReport.py {path} {period} {apass}"
        print(f"Executing report command for {path}/{period}/{apass}/")
        subprocess.run(report_command, shell=True)

def execute_commands(path, period_list, apass, num_threads, rerun):
    # Try to execute for all folders in path if no period list is given
    if not args.period_list :
        period_list = [name for name in os.listdir(f"{path}") if os.path.isdir(path+"/"+name)]
        if not period_list: raise Exception(f"Something went wrong when trying to find periods for {path}/")
        print("[INFO] No period list provided. Running for",period_list)

    # Download from alien
    if args.download:
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as executor:
            futures = []
            for period in period_list: futures.append(executor.submit(download, path, period, apass))
            concurrent.futures.wait(futures)

    # Create _QC.root with plotted TPC histograms
    if args.plot:
        with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
            futures = []
            for period in period_list:
                if not args.apass:
                    apassList = [name for name in os.listdir(f"{path}/{period}/") if os.path.isdir(path+"/"+period+"/"+name)]
                    if not apassList: raise Exception(f"Something went wrong when trying to find apass for {path}/{period}/")
                    for apass in apassList:
                        futures.append(executor.submit(plot, path, period, apass, rerun))
                else:
                    futures.append(executor.submit(plot, path, period, apass, rerun))
            concurrent.futures.wait(futures)

    # Create async reports  
    if args.report:
        if args.path:
            with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
                futures = []
                for period in period_list:
                    if not args.apass:
                        apassList = [name for name in os.listdir(f"{path}/{period}/") if os.path.isdir(path+"/"+period+"/"+name)]
                        if not apassList: raise Exception(f"Something went wrong when trying to find apass for {path}/{period}/")
                        for apass in apassList:
                            futures.append(executor.submit(generate_report, path, period, apass))
                    else:
                        futures.append(executor.submit(generate_report, path, period, apass))
                concurrent.futures.wait(futures)
        else:
            print("Error: Missing path and/or apass arguments for report command")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for executing commands for each period")
    parser.add_argument("period_list", nargs="*", help="List of period strings")
    parser.add_argument("-d", "--download", action="store_true", help="Run download command")
    parser.add_argument("-p", "--plot", action="store_true", help="Run plotter command")
    parser.add_argument("-r", "--report", action="store_true", help="Run report command")
    parser.add_argument("-rr", "--rerun", action="store_true", help="Rerun plotter for existing periods")
    parser.add_argument("--path", help="Path string for generateReport command")
    parser.add_argument("--apass", help="Apass string for generateReport command")
    parser.add_argument("-t", "--num_threads", type=int, default=1, help="Number of threads to be used (default: 1)")
    args = parser.parse_args()
    print(args.path)
    execute_commands(args.path, args.period_list, args.apass, args.num_threads, args.rerun)

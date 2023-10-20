import subprocess
import argparse
import concurrent.futures

def download(path, period, apass):
    download_command = f"python ~/Software/TPCQCVis/TPCQCVis/macro/downloadFromAlien.py {path}/{period}/{apass}/ /alice/data/20{period[3:5]}/{period}/ {apass}"
    print("Executing download command for period:", period)
    subprocess.run(download_command, shell=True)

def plot(path, period, apass, rerun):
    plotter_command = f"python ~/Software/TPCQCVis/TPCQCVis/macro/runPlotter.py {path}/{period}/{apass}/"
    if rerun:
        plotter_command += " --rerun"
    print("Executing plotter command for period:", period)
    subprocess.run(plotter_command, shell=True)

def generate_report(path, period, apass):
    report_command = f"python ~/Software/TPCQCVis/TPCQCVis/macro/generateReport.py {path} {period} {apass}"
    print("Executing report command for period:", period)
    subprocess.run(report_command, shell=True)

def execute_commands(path, period_list, apass, num_threads, rerun):
    if args.download:
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as executor:
            futures = []
            for period in period_list: futures.append(executor.submit(download, path, period, apass))
            concurrent.futures.wait(futures)
            
    if args.plot:
        with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
            futures = []
            for period in period_list: futures.append(executor.submit(plot, path, period, apass, rerun))
            concurrent.futures.wait(futures)
            
    if args.report:
        if args.path and args.apass:
            with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
                futures = []
                for period in period_list: futures.append(executor.submit(generate_report, path, period, apass))
                concurrent.futures.wait(futures)
        else:
            print("Error: Missing path and/or apass arguments for report command")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for executing commands for each period")
    parser.add_argument("period_list", nargs="+", help="List of period strings")
    parser.add_argument("-d", "--download", action="store_true", help="Run download command")
    parser.add_argument("-p", "--plot", action="store_true", help="Run plotter command")
    parser.add_argument("-r", "--report", action="store_true", help="Run report command")
    parser.add_argument("-rr", "--rerun", action="store_true", help="Rerun plotter for existing periods")
    parser.add_argument("--path", help="Path string for generateReport command")
    parser.add_argument("--apass", help="Apass string for generateReport command")
    parser.add_argument("-t", "--num_threads", type=int, default=1, help="Number of threads to be used (default: 1)")
    args = parser.parse_args()

    execute_commands(args.path, args.period_list, args.apass, args.num_threads, args.rerun)

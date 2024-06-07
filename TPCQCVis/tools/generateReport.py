import re
import subprocess
import json
import glob
import argparse
import tempfile
import os
import concurrent.futures

CODEDIR = os.environ['TPCQCVIS_DIR']
DATADIR = os.environ['TPCQCVIS_DATA']
REPORTDIR = os.environ['TPCQCVIS_REPORT']

def replace_in_ipynb(file_path, temp, patterns, replacements):
    # Open the file
    with open(file_path, "r") as file:
        # Read the contents of the file and parse it as JSON
        notebook = json.load(file)

    # Replace the desired parts of the notebook
    for i in range(len(patterns)):
        pattern = patterns[i]
        replacement = replacements[i]
        for cell in notebook["cells"]:
            if "source" in cell:
                cell["source"] = [re.sub(pattern, replacement, line) for line in cell["source"]]

    # Write the modified notebook to the temporary file
    with open(temp, "w") as file:
        json.dump(notebook, file)

def createRunReport(runNumber, period, apass, path, template_path, dir):
    # Create a temporary file with a unique filename for the run report
    with tempfile.NamedTemporaryFile(prefix="TPCQC_", suffix=".ipynb", delete=False) as temp_run:
        temp_run_path = temp_run.name

    print("--> Reporting:", period, apass, runNumber)
    replace_in_ipynb(template_path, temp_run_path,
        ["myPeriod", "myPass", "123456", "myPath"],
        [period, apass, runNumber, path]
    )
    # The command and its arguments for the run report
    run_report_command = [
        "jupyter", "nbconvert", temp_run_path, "--to", "html", "--template", "classic", "--no-input", "--execute",
        "--output", dir + runNumber
    ]
    # Run the command for the run report
    output = subprocess.run(run_report_command, capture_output=True)
    # Check the return code of the command
    if output.returncode == 0:
        # If the command runs successfully
        print("Async QC report generated successfully for runNumber", runNumber)
    else:
        # If the command fails
        print("Error:", output.stderr.decode())
    # Remove the temporary files
    if temp_run_path:
        os.remove(temp_run_path)

def createPeriodReport(period, apass, path, template_path, dir):
    # Create a temporary file with a unique filename for the period report
    with tempfile.NamedTemporaryFile(prefix="TPCQC_", suffix=".ipynb", delete=False) as temp_period:
        temp_period_path = temp_period.name
    replace_in_ipynb(template_path, temp_period_path,
        ["myPeriod", "myPass", "myPath"],
        [period, apass, path]
    )
    # The command and its arguments for the period report
    period_report_command = [
        "jupyter", "nbconvert", temp_period_path, "--to", "html", "--template", "classic", "--no-input", "--execute",
        "--output", dir + period + "_" + apass
    ]
    # Run the command for the period report
    output = subprocess.run(period_report_command, capture_output=True)
    # Check the return code of the command
    if output.returncode == 0:
        # If the command runs successfully
        print("Period QC report generated successfully for period", period, apass)
    else:
        # If the command fails
        print("Error:", output.stderr.decode())
    if temp_period_path:
        os.remove(temp_period_path)

def createComparisonReport(period, apass, path, template_path, dir):
    # Create a temporary file with a unique filename for the comparison report
    with tempfile.NamedTemporaryFile(prefix="TPCQC_", suffix=".ipynb", delete=False) as temp_comparison:
        temp_comparison_path = temp_comparison.name
    replace_in_ipynb(template_path, temp_comparison_path,
        ["myPeriod", "myPass", "myPath"],
        [period, apass, path]
    )
    # The command and its arguments for the comparison report
    comparison_report_command = [
        "jupyter", "nbconvert", temp_comparison_path, "--to", "html", "--template", "classic", "--no-input", "--execute",
        "--output", dir + period + "_" + apass + "_comparison"
    ]
    # Run the command for the comparison report
    output = subprocess.run(comparison_report_command, capture_output=True)
    # Check the return code of the command
    if output.returncode == 0:
        # If the command runs successfully
        print("Comparison QC report generated successfully for period", period, apass)
    else:
        # If the command fails
        print("Error:", output.stderr.decode())
    if temp_comparison_path:
        os.remove(temp_comparison_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for generating Async QC reports")
    parser.add_argument("path", help="Path string")
    parser.add_argument("period", help="Period string")
    parser.add_argument("apass", help="Apass string")
    parser.add_argument("--only_comparison", action="store_true", help="Generate only comparison report")
    parser.add_argument("-t", "--num_threads", type=int, default=1, help="Number of threads to be used (default: 1)")
    args = parser.parse_args()

    run_template_path = f"{CODEDIR}/TPCQCVis/reports/TPC_AQC_Template_Run.ipynb"
    period_template_path = f"{CODEDIR}/TPCQCVis/reports/TPC_AQC_Template_Period.ipynb"
    comparison_template_path = f"{CODEDIR}/TPCQCVis/reports/TPC_AQC_Template_ComparePasses.ipynb"

    if not args.only_comparison:
        fullpath = args.path + "/" + args.period + "/" + args.apass + "/"
        fileList = glob.glob(fullpath + "*_QC.root")
        fileList = [file for file in fileList if file[-13] != "_"]
        fileList.sort()
        runList = [fileList[i][-14:-8] for i in range(len(fileList))]

        # Create run reports in parallel
        print(f"Creating run reports for {args.period} {args.apass}")
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.num_threads) as executor:
            futures = []
            for runNumber in runList:
                futures.append(executor.submit(createRunReport, runNumber, args.period, args.apass, args.path, run_template_path, fullpath))
            concurrent.futures.wait(futures)

        if len(runList) > 1:
            createPeriodReport(args.period, args.apass, args.path, period_template_path, args.path + "/" + args.period + "/")
            
    #### Comparison Report #####
    if len(glob.glob(args.path + "/" + args.period + "/*")) > 1:
        createComparisonReport(args.period, args.apass, args.path, comparison_template_path, args.path + "/" + args.period + "/")
    else:
        print("Not running comparison report, as not enough periods!")

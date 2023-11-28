import re
import subprocess
import json
import glob
import argparse
import tempfile
import os

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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for generating Async QC reports")
    parser.add_argument("path", help="Path string")
    parser.add_argument("period", help="Period string")
    parser.add_argument("apass", help="Apass string")
    args = parser.parse_args()

    template_path = "/home/berki/Software/TPCQCVis/TPCQCVis/reports/TPC_asyncQC_template.ipynb"
    period_template_path = "/home/berki/Software/TPCQCVis/TPCQCVis/reports/TPC_AQC_period_template.ipynb"
    comparison_template_path = "/home/berki/Software/TPCQCVis/TPCQCVis/reports/TPC_AQC_ComparePeriods_template.ipynb"

    # Create a temporary file with a unique filename for the run report
    with tempfile.NamedTemporaryFile(prefix="TPCQC_", suffix=".ipynb", delete=False) as temp_run:
        temp_run_path = temp_run.name

    fullpath = args.path + "/" + args.period + "/" + args.apass + "/"
    fileList = glob.glob(fullpath + "*_QC.root")
    fileList = [file for file in fileList if file[-13] != "_"]
    fileList.sort()
    runList = [fileList[i][-14:-8] for i in range(len(fileList))]

    for runNumber in runList:
        print("--> Reporting:", args.period, args.apass, runNumber)
        replace_in_ipynb(template_path, temp_run_path,
            ["myPeriod", "myPass", "123456", "myPath"],
            [args.period, args.apass, runNumber, args.path]
        )

        # The command and its arguments for the run report
        run_report_command = [
            "jupyter", "nbconvert", temp_run_path, "--to", "html", "--template", "classic", "--no-input", "--execute",
            "--output", fullpath + runNumber
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

    if len(runList) > 1:
        # Create a temporary file with a unique filename for the period report
        with tempfile.NamedTemporaryFile(prefix="TPCQC_", suffix=".ipynb", delete=False) as temp_period:
            temp_period_path = temp_period.name

        replace_in_ipynb(period_template_path, temp_period_path,
            ["myPeriod", "myPass", "myPath"],
            [ args.period, args.apass, args.path]
        )

        # The command and its arguments for the period report
        period_report_command = [
            "jupyter", "nbconvert", temp_period_path, "--to", "html", "--template", "classic", "--no-input", "--execute",
            "--output", fullpath + args.period + "_" + args.apass
        ]

        # Run the command for the period report
        output = subprocess.run(period_report_command, capture_output=True)

        # Check the return code of the command
        if output.returncode == 0:
            # If the command runs successfully
            print("Period QC report generated successfully for period", args.period)
        else:
            # If the command fails
            print("Error:", output.stderr.decode())

        if temp_period_path:
            os.remove(temp_period_path)

        #### Comparison Report #####
        # Create a temporary file with a unique filename for the period report
        with tempfile.NamedTemporaryFile(prefix="TPCQC_", suffix=".ipynb", delete=False) as temp_comparison:
            temp_comparison_path = temp_comparison.name

        replace_in_ipynb(comparison_template_path, temp_comparison_path,
            ["myPeriod", "myPass", "myPath"],
            [ args.period, args.apass, args.path]
        )

        # The command and its arguments for the comparison report
        comparison_report_command = [
            "jupyter", "nbconvert", temp_comparison_path, "--to", "html", "--template", "classic", "--no-input", "--execute",
            "--output", args.path + "/" + args.period + "/" + args.period + "_" + args.apass +"_comparison.html"
        ]

        # Run the command for the comparison report
        output = subprocess.run(comparison_report_command, capture_output=True)

        # Check the return code of the command
        if output.returncode == 0:
            # If the command runs successfully
            print("comparison QC report generated successfully for period", args.period)
        else:
            # If the command fails
            print("Error:", output.stderr.decode())

        if temp_comparison_path:
            os.remove(temp_comparison_path)

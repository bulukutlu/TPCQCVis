import re
import subprocess
import json
import glob
import argparse
import tempfile
import os

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


if __name__ == "__main__":
    template_path = f"{CODEDIR}/TPCQCVis/reports/TPC_AQC_Template_CompareRunToMC.ipynb"

    # Create a temporary file with a unique filename for the run report
    with tempfile.NamedTemporaryFile(prefix="TPCQC_", suffix=".ipynb", delete=False) as temp_run:
        temp_run_path = temp_run.name

    periodList = ["v","y","z","za","zb","zc","zd","zf","zg","zh","zj","zk","zm","zn","zq","zs","zt"]

    for period in periodList:
        print(f"--> Reporting: LHC23{period}")
        replace_in_ipynb(template_path, temp_run_path,
            ["myPeriod"],
            ["LHC23"+period]
        )
        # The command and its arguments for the run report
        run_report_command = [
            "jupyter", "nbconvert", temp_run_path, "--to", "html", "--template", "classic", "--no-input", "--execute",
            "--output", f"/home/berki/Software/TPCQCVis/TPCQCVis/reports/TPC_AQC_LHC23k4g_vs_LHC23{period}.html"
        ]

        # Run the command for the run report
        output = subprocess.run(run_report_command, capture_output=True)

        # Check the return code of the command
        if output.returncode == 0:
            # If the command runs successfully
            print(f"Async QC report generated successfully for LHC23{period}")
        else:
            # If the command fails
            print("Error:", output.stderr.decode())
        
        # Remove the temporary files
        if temp_run_path:
            os.remove(temp_run_path)
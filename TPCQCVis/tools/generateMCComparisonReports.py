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

    ### Part to set
    path = f"{DATADIR}/sim/2024/"
    period = "LHC24e2" 
    passName = "" #keep empty ("") if MC
    pathComparison = f"{DATADIR}/2023/"
    periodListComparison =  ["LHC23zzf","LHC23zzg","LHC23zzh"]
    passNameListComparison = ["apass3","apass3","apass3"]
    #passNameListComparison = ["" for period in periodListComparison] #keep empty ("") if MC

    # Loop over the comparison periods
    for i,periodComparison in enumerate(periodListComparison):
        passNameComparison = passNameListComparison[i]

        print(f"--> Reporting: {period}_{passName} vs {periodComparison}_{passNameComparison}")
        replace_in_ipynb(template_path, temp_run_path,
            ["myPath","myPeriod","myPassName",
             "myComparisonPath","myComparisonPeriod","myComparisonPassName"],
            [path,period,passName,
             pathComparison,periodComparison,passNameComparison]
        )

        # The command and its arguments for the run report
        # Report location
        if passName:
            fileName = f"TPC_AQC_{period}_{passName}_vs_{periodComparison}_{passNameComparison}.html"
        else:
            fileName = f"TPC_AQC_{period}_vs_{periodComparison}_{passNameComparison}.html"
            
        reportPath =  f"{path}/{period}/{passName}/{fileName}"
        print(f"Generating {reportPath}")
        run_report_command = ["jupyter", "nbconvert", temp_run_path, "--to", "html", "--template", "classic", "--no-input", "--execute", "--output", reportPath ]

        # Run the command for the run report
        output = subprocess.run(run_report_command, capture_output=True)

        # Check the return code of the command
        if not output.returncode == 0:
            print("Error:", output.stderr.decode())            
        
        # Remove the temporary files
        if temp_run_path:
            os.remove(temp_run_path)
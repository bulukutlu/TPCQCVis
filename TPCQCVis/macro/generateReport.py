import re
import subprocess
import json
import glob

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

    # Write the modified notebook to the same file
    with open(temp, "w") as file:
        json.dump(notebook, file)


template_path = "/home/berki/Software/TPCQCVis/TPCQCVis/reports/TPC_asyncQC_template.ipynb"
temp_path = "/home/berki/Software/TPCQCVis/TPCQCVis/reports/TPC_asyncQC_template_tmp.ipynb"

period = "LHC22s"
apass = "apass4"
path = "/mnt/cave/alice/data/2022/"
fullpath = path+"/"+period+"/"+apass+"/"
fileList = glob.glob(fullpath+"*_QC.root")
fileList.sort()
runList = [fileList[i][-14:-8] for i in range(len(fileList))]
#runList = ["523141","523142","523148","523182","523186","523298","523306","523308","523309","523397","523399","523401","523441","523541","523559","523669","523671","523677","523728","523731","523779","523783","523786","523788","523789","523792","523797","523821","523897"]

for runNumber in runList:
    #runNumber = "523677"
    replace_in_ipynb(template_path,temp_path, 
        ["myPeriod","myPass","123456","myPath"],
        ["\""+period+"\"","\""+apass+"\"",runNumber,"\""+path+"\""]
        )

    # the command and its arguments
    command = ["jupyter", "nbconvert",temp_path, "--to", "html", "--template", "classic", "--no-input", "--execute",
            "--output", fullpath+runNumber]

    # Run the command
    output = subprocess.run(command, capture_output=True)

    # check the return code of command
    if output.returncode == 0:
        # if command runs successfully
        print("Async QC report generated successfully, runNumber "+runNumber)
    else:
        #if command failed
        print("Error:",output.stderr.decode())
import os
import subprocess
import sys
import time

def getRunList(remote_dir):
    directories = subprocess.run(["alien_ls", remote_dir], capture_output=True)
    # Remove unnecessary characters and split the string into individual elements
    cleaned_string = directories.stdout.decode("utf-8").replace("b'", "").replace("\n", "").replace("'", "")
    elements = cleaned_string.split("/")
    runList = [element for element in elements if (element and (int(element)>100000))]
    print("Found",len(runList),"runs in", remote_dir)
    return runList

def downloadFiles(local_dir, remote_dir, production, runList):
    from TPCQCVis.src.utility import downloadAttempts

    for run in runList:
        print("Searching run",run)
        target = subprocess.run(["alien.py", "find", remote_dir + run + "/" + production + "/QC/001/", "QC.root"], capture_output=True)
        if len(target.stdout) > 0:
            target_path = target.stdout[:-1].decode('UTF-8')
            if target_path[0] == " ":
                target_path = target_path[1:]
            print("Downloading \"" + target_path+"\"")
            downloadAttempts(target_path, local_dir + run + ".root", 5)
            #subprocess.run(["alien.py", "cp", "alien:" + target_path, "file:" + local_dir + run + ".root"])
        else:
            target = subprocess.run(["alien.py", "find", remote_dir + run + "/" + production + "/", "QC_fullrun.root"], capture_output=True)
            if len(target.stdout) > 0:
                target_path = target.stdout[:-1].decode('UTF-8')
                print("Downloading " + target_path)
                downloadAttempts(target_path, local_dir + run + ".root", 5)
                if False:
                    slices = subprocess.run(["alien.py", "find", remote_dir + run + "/" + production + "/", "/QC/001/QC.root"], capture_output=True)
                    slices_path = slices.stdout[:-1].decode('UTF-8').splitlines()
                    if len(slices_path) > 1:
                        print("Downloading all time slices as well.")
                        for path in slices_path:
                            timestamp = path[-(len("/QC/001/QC.root")+4):-len("/QC/001/QC.root")]
                            downloadAttempts(path, local_dir + run + "_" + timestamp +".root", 5)
                            #subprocess.run(["alien.py", "cp", "alien:" + path, "file:" + local_dir + run + "_" + timestamp +".root"])
            else:
                print("File " + remote_dir + run + "/" + production + "/QC/001/QC.root" + " not found!")
        time.sleep(1) #otherwise too many requests

# Get the command-line arguments
args = sys.argv[1:]

# Check if the correct number of arguments is provided
if len(args) < 3:
    print("Insufficient number of arguments provided.")
    print("Usage: python downloadFromAlien.py local_dir remote_dir production [runList]")
    sys.exit(1)

# Assign the command-line arguments to variables
local_dir = args[0]
remote_dir = args[1]
production = args[2]
runList = args[3:] if len(args) > 3 else None

# If runList is not provided, obtain it dynamically
if runList is None:
    runList = getRunList(remote_dir)

# Download files
downloadFiles(local_dir, remote_dir, production, runList)
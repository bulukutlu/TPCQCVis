import os.path
import argparse
import base64
import json
import re
import time
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
import logging
import requests
import datetime
import os
import sys
import subprocess
import ROOT
import glob
from array import array
import argparse
import concurrent.futures
import schedule
import time

LOCALDIR = "/cave/alice-tpc-qc/data/sim/"

def getPaths(path):
    runs = subprocess.run(["alien_ls", path], capture_output=True)
    runs = runs.stdout.splitlines()
    print("Found", len(runs), "runs")
    return [path+run.decode('UTF-8')+"QC" for run in runs]

def downloadFromAlien(new_productions):
    # Downloading from alien
    downloadedFiles = []
    for i,path in enumerate(new_productions):
        year = path.split("/")[3]
        period = path.split("/")[4]
        runNumber = path.split("/")[6]
        target_path = path + "/tpcStandardQC.root"
        local_path  = LOCALDIR+"/"+year+"/"+period+"/"+runNumber+".root"
        if not i: print("Downloading to", local_path)
        print("Downloading " + target_path)
        #print("Executing:",["alien.py", "cp", "alien:" + target_path, "file:"+local_path])
        subprocess.run(["alien.py", "cp", "alien:" + target_path, "file:"+local_path])
        time.sleep(3)
        if os.path.isfile(local_path):
            downloadedFiles.append(local_path)
        else:
            print("File which should have been downloaded does not exit:", local_path)
    return downloadedFiles

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for reading list of alien paths containing simulation QC, and downloading.")
    parser.add_argument("--paths",nargs="*",type=str, help="Paths of merged QC output")
    parser.add_argument("--dir",type=str, help="Path to dir of all runs (e.g. /alice/sim/2024/LHC24d2b/0/)")
    args = parser.parse_args()

    fileList = []
    if args.dir:
        fileList = getPaths(args.dir)
    else:
        fileList = args.paths
    downloadedFiles = downloadFromAlien(fileList)
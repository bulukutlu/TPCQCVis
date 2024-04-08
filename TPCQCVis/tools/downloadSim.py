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

def downloadFromAlien(new_productions):
    # Downloading from alien
    downloadedFiles = []
    for path in new_productions:
        year = path.split("/")[3]
        period = path.split("/")[4]
        runNumber = path.split("/")[6]
        target_path = path + "/tpcStandardQC.root"
        local_path  = LOCALDIR+"/"+year+"/"+period+"/"+runNumber+".root"
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
    args = parser.parse_args()

    downloadedFiles = downloadFromAlien(args.paths)
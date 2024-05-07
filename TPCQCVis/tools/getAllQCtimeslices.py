import os.path
import argparse
import base64
import json
import re
import time
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
import pathlib


def downloadFiles(path):
    print()
    target = subprocess.run(["alien.py", "find", path ,"/QC/QC.root"], capture_output=True)
    if len(target.stdout) > 0:
        target_paths = target.stdout.splitlines()
        for target_path in target_paths:
            target_path = target_path.decode('UTF-8')
            print("Downloading \"" + target_path+"\"")
            timeSlice = target_path.split("/")[-3]
            subprocess.run(["alien.py", "cp", "alien:" + target_path, f"file:{timeSlice}.root"])

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for reading MonaLisa mail for daily finished jobs and execute TPC async QC")
    parser.add_argument("--path", help="path to dir")

    args = parser.parse_args()  
    downloadFiles(args.path)
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

SCOPES = ['https://www.googleapis.com/auth/gmail.readonly','https://www.googleapis.com/auth/gmail.modify']
LOCALDIR = "/cave/alice-tpc-qc/data/"

# Access GMAIL and read daily productions
def readDailyReport(sender="",date="",onlyUnread=False):
    """Shows basic usage of the Gmail API.
    Lists the user's Gmail labels.
    """
    criteria = ""
    if onlyUnread: criteria="is:unread"
    
    new_productions = []

    creds = None
    # The file token.json stores the user's access and refresh tokens, and is
    # created automatically when the authorization flow completes for the first
    # time.
    if os.path.exists('token.json'):
        creds = Credentials.from_authorized_user_file('token.json', SCOPES)
    # If there are no (valid) credentials available, let the user log in.
    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            flow = InstalledAppFlow.from_client_secrets_file('~/Software/TPCQCVis/credentials.json', SCOPES)
            creds = flow.run_local_server(port=0)
        # Save the credentials for the next run
        with open('token.json', 'w') as token:
            token.write(creds.to_json())
    try:
        # Call the Gmail API
        service = build('gmail', 'v1', credentials=creds)
        results = service.users().messages().list(userId='me', labelIds=['INBOX'], q=criteria).execute()
        messages = results.get('messages',[]);
        if not messages:
            print('No messages.')
        else:
            message_count = 0
            for message in messages:
                msg = service.users().messages().get(userId='me', id=message['id']).execute()                
                email_data = msg['payload']['headers']
                email_sender = [values["value"] for values in email_data if values["name"] == "From"][0]
                if sender and (sender in email_sender):
                    email_date = [values["value"] for values in email_data if values["name"] == "Date"][0]
                    email_subject = [values["value"] for values in email_data if values["name"] == "Subject"][0]
                    if (not date) or (date in email_subject):
                        print(f"Read following email (ID:{message['id']}):")
                        print("Sender:",sender, "Date:", email_date, "Subject:", email_subject)
                        for i,part in enumerate(msg['payload']['parts']):
                            try:
                                data = part['body']["data"]
                                byte_code = base64.urlsafe_b64decode(data)
                                text = byte_code.decode("utf-8").split("MonteCarlo")[0]
                                if i == 0: 
                                    paths = str(text).split("path:")[1:]
                                    for path in paths:
                                        line = path.split("\n")[0].split("QC")[0]+"QC/"
                                        new_productions.append(line)

                                # mark the message as read (optional)
                                #msg  = service.users().messages().modify(userId='me', id=message['id'], body={'removeLabelIds': ['UNREAD']}).execute()                                                       
                            except BaseException as error:
                                pass
                        break
    except Exception as error:
        print(f'An error occurred: {error}')
        
    if len(new_productions):
        print("Found following new productions:")
        for prod in new_productions:
            print(" + "+prod)
    else:
        print("No new productions found!")
    
    return new_productions

def downloadFromAlien(new_productions):
    # Downloading from alien
    downloadedFiles = []
    for path in new_productions:
        year = path.split("/")[3]
        period = path.split("/")[4]
        apass = path.split("/")[6]
        runNumber = path.split("/")[5]
        target_path = path + "001/QC_fullrun.root"
        local_path  = LOCALDIR+"/"+year+"/"+period+"/"+apass+"/"+runNumber+".root"
        print("Downloading " + target_path)
        #print("Executing:",["alien.py", "cp", "alien:" + target_path, "file:"+local_path])
        subprocess.run(["alien.py", "cp", "alien:" + target_path, "file:"+local_path])
        time.sleep(1)
        if os.path.isfile(local_path):
            downloadedFiles.append(local_path)
        else:
            print("File which should have been downloaded does not exit:", local_path)
    return downloadedFiles

def plotQCfiles(paths, num_threads):
    def plot(path):
        plotter_command = f"python ~/Software/TPCQCVis/TPCQCVis/macro/runPlotter.py {path} --target {path}"
        print(f"Executing plotter command for {path}")
        subprocess.run(plotter_command, shell=True)

    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
        futures = []
        for path in paths:
            if os.path.isfile(path):
                #plot(path)
                #print("Plotting ", path)
                futures.append(executor.submit(plot, path))
            else:
                print("Bad path given: ", path)
        concurrent.futures.wait(futures)        

def reportTPCAsyncQC(paths, num_threads):
    def generate_report(path, period, apass):
        report_command = f"python ~/Software/TPCQCVis/TPCQCVis/macro/generateReport.py {path} {period} {apass}"
        print(f"Executing report command for {path}/{period}/{apass}/")
        subprocess.run(report_command, shell=True)

    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
        unique = list(set(["/"+os.path.join(*(path.split("/")[:-1]))+"/" for path in paths]))
        futures = []
        for entry in unique:
            path = "/"+os.path.join(*entry.split("/")[:-3])+"/"
            period = entry.split("/")[-3]
            apass = entry.split("/")[-2]
            if os.path.isdir(entry):
                print("Reporting ", entry)
                futures.append(executor.submit(generate_report, path, period, apass))
            else:
                print("Bad path given: ", entry)
        concurrent.futures.wait(futures)

def createMessage():
    import os
    def generateLink(path):
        link = "https://alice-tpc-qc.web.cern.ch/reports/"+path[path.find('202'):]
        return link
    
    dirs = []
    files = []
    directory = "/cave/alice-tpc-qc/data/2023/" #LOCALDIR
    for root, dirnames, filenames in os.walk(directory):
        for filename in filenames:
            if filename.endswith('.html'):
                fname = os.path.join(root, filename)
                dirs.append(root)
                files.append(fname)
    dirs = list(set(sorted(dirs)))
    files = sorted(files)

    now = datetime.datetime.now()
    myText = ["## Daily TPC Async QC Report - "+now.strftime("%d.%m.%Y")+"\n"]
    for di in dirs:
        myTitle = "#### ["+di[di.find("LHC"):]+"]("+generateLink(di)+")\n"
        myText.append(myTitle)
        myList = ""
        for file in files:
            if di in file and "/" not in file[len(di)+1:]:
                myList += ("["+file.split("/")[-1]+"]("+generateLink(file)+") - ")
        myText.append(myList[:-3])
    myText = '\n'.join(map(str,myText))
    if not files:
        myText += "No new files processed."
    return str(myText)

def sendMessageToMattermost(myMessage):
    headers = {'Content-Type': 'application/json',}
    values = '{ "text": \"'+myMessage+'\" }'
    print("Sending following message:\n"+myMessage)
    response = requests.post("https://mattermost.web.cern.ch/hooks/krtdox9rbtgsxgqif3ijy51y8c",headers=headers, data=values)
    print(response)

def main(date, threads):
    print(f"--> Running main(date={date}, threads={threads})")
    # Read Email
    new_productions = readDailyReport("berkin.ulukutlu@cern.ch", date, onlyUnread=False)
    # Download
    downloadedFiles = downloadFromAlien(new_productions)
    # Plot
    plotQCfiles(downloadedFiles, threads)
    # Create reports
    reportTPCAsyncQC(downloadedFiles, threads)
    # Make message from created reports
    mattermostMessage = createMessage()
    # Move reports
    move_command = f"python ~/Software/TPCQCVis/TPCQCVis/macro/moveFiles.py -i /cave/alice-tpc-qc/data/ -o /cave/alice-tpc-qc/reports/ -p '*.html'"
    subprocess.run(move_command, shell=True)
    # rsync
    sync_command = "gpg -d -q ~/.myssh.gpg | sshpass rsync -hvrPt /cave/alice-tpc-qc/reports/ lxplus:/eos/project-a/alice-tpc-qc/www/reports/"
    subprocess.run(sync_command, shell=True)
    # Update server
    update_command = "gpg -d -q ~/.myssh.gpg | sshpass ssh lxplus 'python /eos/project-a/alice-tpc-qc/www_resources/updateServer.py'"
    subprocess.run(update_command, shell=True)
    # Send mattermost message
    sendMessageToMattermost(mattermostMessage)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for reading MonaLisa mail for daily finished jobs and execute TPC async QC")
    parser.add_argument("--date", help="Date from which to read the daily report")
    parser.add_argument("-t", "--num_threads", type=int, default=1, help="Number of threads to be used (default: 1)")

    args = parser.parse_args()
    threads = args.num_threads
    if not args.date:
        date = datetime.date.today().strftime("%d.%m.%Y")
    else:
        date = args.date

    main(date, threads)

    

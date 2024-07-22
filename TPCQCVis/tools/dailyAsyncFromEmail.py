import os.path
import argparse
import base64
import time
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
import requests
import datetime
import os
import subprocess
import argparse
import concurrent.futures
import schedule
import time
import itertools

SCOPES = ['https://www.googleapis.com/auth/gmail.readonly','https://www.googleapis.com/auth/gmail.modify']

CODEDIR = os.environ['TPCQCVIS_DIR']
DATADIR = os.environ['TPCQCVIS_DATA']
REPORTDIR = os.environ['TPCQCVIS_REPORT']

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
    if os.path.exists(CODEDIR+'token.json'):
        creds = Credentials.from_authorized_user_file(CODEDIR+'token.json', SCOPES)
    # If there are no (valid) credentials available, let the user log in.
    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            os.remove(CODEDIR+'token.json')
            #creds.refresh(Request())    
        else:
            flow = InstalledAppFlow.from_client_secrets_file(CODEDIR+'credentials.json', SCOPES)
            creds = flow.run_local_server(port=0)
        # Save the credentials for the next run
        with open(CODEDIR+'token.json', 'w') as token:
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
                                        if line[0] == " ":
                                            line = line[1:]
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
    # Function to reliably download files from alien
    def downloadAttempts(target_path, local_path, nDownloadAttempts):
        message = "ERROR"
        attempt = 0
        while ("ERROR" in message and attempt < nDownloadAttempts):
            attempt += 1
            result = subprocess.run(["alien.py", "cp", "alien:" + target_path, "file:"+local_path], capture_output=True)
            message = result.stdout.decode()
            print("\033[1m Attempt",attempt,":\033[0m",message.replace('\n', ' '))
        if attempt == nDownloadAttempts:
            print(f"Download failed after {nDownloadAttempts} attempts. Moving on.")

    # Downloading from alien
    downloadedFiles = []
    for path in new_productions:
        year = path.split("/")[3]
        period = path.split("/")[4]
        apass = path.split("/")[6]
        runNumber = path.split("/")[5]
        target = subprocess.run(["alien.py", "find", path, "QC_fullrun.root"], capture_output=True)
        if len(target.stdout) > 0: target_path = target.stdout[:-1].decode('UTF-8')
        else:
            print("No file found for:", path)
            continue
        local_path  = f"{DATADIR}/{year}/{period}/{apass}/{runNumber}.root"
        print("Downloading \"" + target_path + "\"")
        nDownloadAttempts = 5
        downloadAttempts(target_path, local_path, nDownloadAttempts)
        time.sleep(1)
        if os.path.isfile(local_path):
            downloadedFiles.append(local_path)
        else:
            print("File which should have been downloaded does not exit:", local_path)
            
    return downloadedFiles

def plotQCfiles(paths, num_threads):
    def plot(path):
        plotter_command = f"python {CODEDIR}/TPCQCVis/tools/runPlotter.py {path} --target {path}"
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
    def generate_report(path, period, apass, num_threads):
        report_command = f"python {CODEDIR}/TPCQCVis/tools/generateReport.py {path} {period} {apass} -t {num_threads}"
        print(f"Executing report command for {path}/{period}/{apass}/")
        subprocess.run(report_command, shell=True)

    with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
        unique = list(set(["/"+os.path.join(*(path.split("/")[:-1]))+"/" for path in paths]))
        futures = []
        parallelThreads = max(1, num_threads//len(unique)) # Otherwise they try to get too many threads
        for entry in unique:
            path = "/"+os.path.join(*entry.split("/")[:-3])+"/"
            period = entry.split("/")[-3]
            apass = entry.split("/")[-2]
            if os.path.isdir(entry):
                print("Reporting ", entry)
                futures.append(executor.submit(generate_report, path, period, apass, parallelThreads))
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
    directory = DATADIR
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

def catchUp(new_productions, threads):
    # Confirm path structure: /alice/data/YEAR/PERIOD/RUN/APASS/time/QC/
    if "LHC" not in new_productions[0].split("/")[4] or "pass" not in new_productions[0].split("/")[6]:
        print("Path structure is not as expected. Exiting.")
        return
    # Extract unique period - pass pairs
    unique_PeriodPass = list(set([path.split("/")[4]+"/"+path.split("/")[6] for path in new_productions]))
    unique_PeriodPass = sorted(unique_PeriodPass)    
    print("\n--> CATCH-UP: ", unique_PeriodPass)
    for period_pass in unique_PeriodPass:
        period = period_pass.split("/")[0]
        apass = period_pass.split("/")[1]
        year = "20"+period.split("LHC")[1][:2]
        print(f"Processing {year}/{period}/{apass}")
        subprocess.run(f"python {CODEDIR}/TPCQCVis/tools/qc_master.py -t {threads} --path {DATADIR}/{year} --apass {apass} --download --plot --report {period}", shell=True)

def printDurations(durations):
    print("\n\n ### Durations:")
    for key in durations:
        print(f" + {key}: {durations[key]:.2f} seconds")
    #Print total duration
    print(f" + Total: {sum(durations.values()):.2f} seconds")

def main(date=None, threads=1, mattermost=False, no_plot=False, no_report=False, no_upload=False):
    if not date:
        date = datetime.date.today().strftime("%d.%m.%Y")
    print(f"\n\n\n ### Running main(date={date}, threads={threads})")
    durations = {}
    # Measure run duration for each step
    start = time.time()
    # Read Email
    new_productions = readDailyReport("berkin.ulukutlu@cern.ch", date, onlyUnread=True)
    if new_productions:
        # Download
        downloadedFiles = downloadFromAlien(new_productions)
        durations["Download"] = time.time() - start

        if no_plot:
            printDurations(durations)
            return
        # Plot
        plotQCfiles(downloadedFiles, threads)
        durations["Plot"] = time.time() - start - durations["Download"]

        if no_report:
            printDurations(durations)
            return
        # Create reports
        reportTPCAsyncQC(downloadedFiles, threads)
        durations["Report"] = time.time() - start - durations["Download"] - durations["Plot"]

        # Make message from created reports
        mattermostMessage = createMessage()
        # Move reports
        move_command = f"python {CODEDIR}/TPCQCVis/tools/moveFiles.py -i {DATADIR} -o {REPORTDIR} -p '*.html'"
        subprocess.run(move_command, shell=True)
        
        if no_upload:
            printDurations(durations)
            return
        # rsync
        sync_command = f"gpg -d -q ~/.myssh.gpg | sshpass rsync -hvrPt {REPORTDIR} lxplus:/eos/project-a/alice-tpc-qc/www/reports/"
        subprocess.run(sync_command, shell=True)
        # Update server
        update_command = "gpg -d -q ~/.myssh.gpg | sshpass ssh lxplus8 'python2 /eos/project-a/alice-tpc-qc/www_resources/updateServer.py'"
        subprocess.run(update_command, shell=True)
        durations["Upload"] = time.time() - start - durations["Download"] - durations["Plot"] - durations["Report"]
        if mattermost:
            # Send mattermost message
            sendMessageToMattermost(mattermostMessage)
        
        printDurations(durations)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script for reading MonaLisa mail for daily finished jobs and execute TPC async QC")
    parser.add_argument("--date", help="Date from which to read the daily report")
    parser.add_argument("--dates",nargs="*",type=str, help="Dates from which to read the daily report")
    parser.add_argument("-t", "--num_threads", type=int, default=1, help="Number of threads to be used (default: 1)")
    parser.add_argument("-s", "--schedule", type=str, default=0, help="Schedule daily execution of reports, give time (e.g. 1030)")
    parser.add_argument("-m", "--mattermost", action="store_true", help="Send message to mattermost")
    parser.add_argument("--no_plot", action="store_true", help="Don't create _QC.root files")
    parser.add_argument("--no_report", action="store_true", help="Don't create reports")
    parser.add_argument("--no_upload", action="store_true", help="Don't upload reports")
    parser.add_argument("--catch_up", action="store_true", help="Catch up with the missed productions (make sure everything is complete")
    args = parser.parse_args()
    threads = args.num_threads
    if not args.date:
        date = datetime.date.today().strftime("%d.%m.%Y")
    else:
        date = args.date

    # Schedule the daily execution
    if args.schedule:
        schedule.every().day.at(args.schedule[:2] + ":" + args.schedule[2:]).do(main, threads=threads, mattermost=args.mattermost, no_plot=args.no_plot,  no_report=args.no_report, no_upload=args.no_upload)
        while True:
            schedule.run_pending()
            time.sleep(60)

    # Run for multiple dates
    elif args.dates:
        # Format the dates list
        if len(args.dates) == 1 and "-" in args.dates[0]:
            start = args.dates[0].split("-")[0]
            end = args.dates[0].split("-")[1]
            start = datetime.datetime.strptime(start, "%d.%m.%Y")
            end = datetime.datetime.strptime(end, "%d.%m.%Y")
            print(start, end)
            dates = [(start + datetime.timedelta(days=x)).strftime("%d.%m.%Y") for x in range(0, (end-start).days)]
            print("Running for following dates:", dates)
        else:
            dates = args.dates
        
        # Run catch up
        if args.catch_up:
            all_productions = [readDailyReport("berkin.ulukutlu@cern.ch", date, onlyUnread=True) for date in dates]
            all_productions = list(itertools.chain.from_iterable(all_productions))
            catchUp(all_productions, threads)
        else: #Run normally
            for date in dates:
                main(date=date, threads=threads, mattermost=args.mattermost, no_plot=args.no_plot,  no_report=args.no_report, no_upload=args.no_upload)
    
    # Run for a single date
    else:
        # Run catch up
        if args.catch_up:
            all_productions = readDailyReport("berkin.ulukutlu@cern.ch", date, onlyUnread=True)
            catchUp(all_productions, threads)
        else: #Run normally
            main(date=date, threads=threads, mattermost=args.mattermost, no_plot=args.no_plot, no_report=args.no_report, no_upload=args.no_upload)
        

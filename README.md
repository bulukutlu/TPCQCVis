# TPCQCVis
## Index:
1. [Introduction](#introduction)
1. [Setting up](#setting-up)
1. [User Guide](#user-guide)
2. [Developers Guide](#user-guide)

## Introduction:
The monitoring of the ALICE TPC quality control data in RUN3 is planned to be done using the Jupyter environment, where libraries e.g. Bokeh plotter can be used which enables very user-friendly interactivity capabilities. We have PyRoot based notebooks for visualizing the Root Object outputs from central sync and async QC. And, we have expert dashboards made with RootInteractive using skimmed data.

The webinterface for generated reports is located at [alice-tpc-qc.web.cern.ch](https://alice-tpc-qc.web.cern.ch/)

## Setting up:
1. Install [O2](https://alice-doc.github.io/alice-analysis-tutorial/building/custom.html) & [Quality Control](https://github.com/AliceO2Group/QualityControl)
2. Enter QC environment
   1. Install TPCQCVis
      1. Clone this repo.
         1. `git clone https://github.com/bulukutlu/TPCQCVis.git`
      2. Set the following environment variables in your `.bashrc` or `.bash_profile` etc.:
          - `TPCQCVIS_DIR`: Directory path where the code resides.
          - `TPCQCVIS_DATA`: Directory path where data will be downloaded.
          - `TPCQCVIS_REPORT`: Directory path where reports will be stored.
      3. Install the package: `pip install -e $TPCQCVIS_DIR`
   2. [Optional] Install [RootInteractive](https://github.com/miranov25/RootInteractive)

## User guide:
### Creating reports with templates
The worflow for the report creation is as follows:
1. During offline production async QC tasks are run. A QC merger process gathers all files for a run in to on file. The produced QC output goes into the QCDB (which can be visualized via the QCG) but also to the alien with the name `QC_fullrun.root`
2. We download the `QC_fullrun.root` objects from alien.
    > Implemented in `tools/downloadFromAlien.py`
3. We extract the TPC relevant objects and write them to a new root file. We also run postprocessing on this file.
    > Implemented in `tools/runPlotter.py`
4. We create reports using jupyter notebook templates with the QC files as input.
    > Implemented in `tools/generateReport.py`
5. The generated reports are uploaded to the eos project folder (located at `/eos/project-a/alice-tpc-qc/www` also accesible via CernBox). The WebServices handle the displaying of the reports for viewers.
    > Implemented in `tools/syncAndUpload.py`

To avoid having to go through all of the steps above everytime, there are automation scripts implemented.
### Daily Async Report
```
python $TPCQCVIS_DIR/TPCQCVis/tools/dailyAsyncFromEmail.py [--date DATE] [--dates DATES...] [--num_threads NUM_THREADS] [--schedule SCHEDULE] [--mattermost]
```

> Every day at 10:00 CERN time the MonALISA interface sends an email to the alice-dpg-async-qc mailing group the list of async productions completed that day.

The `tools/dailyAsyncFromEmail.py` script allows the automatic generation of reports for the productions of a given day.

- Before running the script, you need to authorize access to Gmail by following these steps:
   - Get the login information for the Gmail account used to fetch the reminder emails from @bulukutlu
   - Place the generated `credentials.json` file in the directory specified by `TPCQCVIS_DIR`.
   - When running the script initially, it will prompt you to log in to the Google account and authorize access.

**Options:**
  - `-num_threads`: how many threads to use when running in parallel (downloading still done sequentially due to LRZ limitations)
  - `-date`: specify date (example: `--date 08.04.2024`)
  - `-dates`: specify dates (example: `--dates 06.04.2024 07.04.2024 08.04.2024`)
  - `--schedule`: Schedule automatic running of the script everyday at specified time (example: `--schedule 1015`)
  - `--mattermost`: Send overview of generated reports to TPC-AsyncQC channel
  

### QC Master Automation
```
python $TPCQCVIS_DIR/TPCQCVis/tools/qc_master.py [--path PATH] [--apass APASS] [-d] [-p] [-r] [-rr] [-t NUM_THREADS] [period_list...]
```
This script allow the calling of different parts of the production chain for multiple different periods automatically.

**Options:**
- period_list: List of period strings.
- `-d` or `--download`: Run the download command.
- `-p` or `--plot`: Run the plotter command.
- `-r` or `--report`: Run the report command.
- `-rr` or `--rerun`: Rerun plotter for existing periods.
- `--path`: Path string for the generateReport command.
- `--apass`: Apass string for the generateReport command.
- `-t` or `--num_threads`: Number of threads to be used - (default: 1).

### MonteCarlo
The QC output of the MonteCarlo productions is a bit differently structured, as such the downloading has to be handled with a different script.

```
python $TPCQCVIS_DIR/TPCQCVis/tools/downloadSim.py [alien paths ...]
```
After downloading the files and cd'ing to their location, the QC plot files can again be generated using:
```
python $TPCQCVIS_DIR/TPCQCVis/tools/runPlotter.py -t 10 $PWD/
```
To compare MC against the data runs they were anchored to. For this a report template exist at: `reports/TPC_AQC_Template_CompareRunToMC.ipynb`
> [!TIP]
> Normally, the MC runs are very stable and don't need to be checked on their own (unless requested). If you want, you can generate the normal QC reports using the `tools/generateReport.py` script.

To convert the saved notebook into html report see [Exporting Notebooks](#exporting-notebooks)

## Developers guide:
This repository mainly contains the automation scripts described above and ROOT functions for making nicer plots and postprocessing.

- The implementation is located at `$TPCQCVIS_DIR/TPCQCVis/src`.
- For testing functionality there are notebooks located at `$TPCQCVIS_DIR/TPCQCVis/tests`.
- Notebook demonstrating some core functions [UserGuide.ipynb](TPCQCVis/tutorials/UserGuide.ipynb)
- Other demos can be found at `TPCQCVis/tutorials/` (not up to date)
 
### Exporting notebooks:
#### As report:
Command to run:
```
jupyter nbconvert myNotebook.ipynb --to html --template classic --no-input
```
> Remove the `--no-input` to have the code also in the report

#### As slides:
Command to run:
```
jupyter nbconvert myNotebook.ipynb --to slides --no-input --SlidesExporter.reveal_scroll=True
```

Then open the html file with text editor and change the initializer of reveal to this:
```
Reveal.initialize({
            controls: true,
            progress: true,
            history: false,
            transition: "slide",
            slideNumber: "true",
            viewDistance: 50,
            mobileViewDistance: 20,
            preloadIframes: true,
            autoPlayMedia:true,
            plugins: [RevealNotes]
        });
```
This makes it possible that all the plots in the slides will be loaded automatically when file is opened on browser. (works on Chrome, still didn't get the expected behavior in Firefox)

### Interactive Dashboards
> [!NOTE]
> It is planned to also provide interactive dashboards generated using RootInteractive for TPC QC. These will be based on skimmed AO2D and reconstructed data, as well as timeseries and aggregated run informations collected from CCDB and QCDB.
> Currently this project is [WIP] and no automatized dashboards are yet being created.

For the development, some example dashboard templates can be found at `/TPCQCVis/dashboards/`


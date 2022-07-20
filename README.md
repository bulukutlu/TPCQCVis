# TPCQCVis
## Index:
1. [Introduction](#introduction)
1. [Setting up](#setting-up)
1. [User Guide](TPCQCVis/tutorials/UserGuide.ipynb)
1. [Demos](TPCQCVis/tutorials/)

## Introduction:
The monitoring of the ALICE TPC quality control data in RUN3 is planned to be done using the Jupyter environment, where libraries e.g. Bokeh plotter can be used which enables very user-friendly interactivity capabilities. We have PyRoot based notebooks for visualizing the Root Object outputs from central sync and async QC. And, we have expert dashboards made with RootInteractive using skimmed data.

## Setting up:
1. Install Quality Control
2. Install RootInteractive

## Exporting notebooks:
### As report:
Command to run:
```
jupyter nbconvert myNotebook.ipynb --to html --template classic --no-input
```
> Remove the `--no-input` to have the code also in the report

### As slides:
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

This information is depriciated, will update ASAP

# TPC_QC_Visualization
[![Binder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/gh/bulukutlu/TPC_QC_Visualization/master)

You can browse all the jupyter notebooks without installing anything using [binder](https://mybinder.org/v2/gh/bulukutlu/TPC_QC_Visualization/master).

## Index:
1. [Introduction](#introduction)
1. [Setting up](#setting-up)
1. [User Guide](TPC_QC_Visualization/UserGuide.ipynb)
1. [Dashboard Demo](TPC_QC_Visualization/dashboard_demo.ipynb)

## Introduction:
The monitoring of the ALICE TPC quality control data in RUN3 is planned to be done using the Jupyter environment, where libraries e.g. Bokeh plotter can be used which enables very user-friendly intrectivity capabilities. This notebook aims to introduce new users to the jupyter notebook environment and the other involved libraries by giving multiple examples relavent to TPC QC visualisation.

## Setting up:
For testing and development local installation of some libraries are required. These include; [Pandas](https://pandas.pydata.org/docs/user_guide/index.html), [Numpy](https://numpy.org/doc/stable/), [Uproot](https://github.com/scikit-hep/uproot), [Bokeh](https://docs.bokeh.org/en/latest/docs/user_guide.html), [Holoviews](http://holoviews.org/user_guide/index.html), [Datashader](https://datashader.org/user_guide/index.html), [Panel](https://panel.holoviz.org/) and [RootInteractive](https://github.com/miranov25/RootInteractive). For the easiest installation we recommend [creating an Anaconda environment](https://www.anaconda.com/products/individual).
### Installing the packages in Anaconda:
```console
conda install pandas
conda install numpy
conda install -c conda-forge uproot
conda install bokeh
conda install -c pyviz holoviews bokeh
conda install -c conda-forge datashader
conda install -c conda-forge panel
```
### Installing packages using pip:
If pip is not already installed:
```console
sudo apt update
sudo apt install python3-pip
```
```console
pip install pandas
pip install uproot
pip install numpy
pip install bokeh
pip install "holoviews[recommended]"
pip install datashader
pip install panel
```

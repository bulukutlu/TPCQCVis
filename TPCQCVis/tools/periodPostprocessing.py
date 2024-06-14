import ROOT
import glob
import math
import sys
import numpy as np
import pandas as pd
import ipywidgets as widgets
from copy import copy
import argparse

from TPCQCVis.src.drawHistograms import *
from TPCQCVis.src.drawTrending import *
from TPCQCVis.src.drawMultiTrending import *
from TPCQCVis.src.checkHistograms import *
from TPCQCVis.src.checkTrending import *
from statistics import median

def getMedianHistogram(hists):
    def getAxisParam(hist, axis="x"):
        if "TH1" in str(type(hist)):
            xBins = hist.GetXaxis().GetXbins().GetArray()
            if len(xBins) == 0:
                return hist.GetNbinsX(), hist.GetXaxis().GetXmin(), hist.GetXaxis().GetXmax()
            else:
                return hist.GetNbinsX(), xBins
        elif "TH2" in str(type(hist)):
            if axis == "x":
                xBins = hist.GetXaxis().GetXbins().GetArray()
                if len(xBins) == 0:
                    return hist.GetNbinsX(), hist.GetXaxis().GetXmin(), hist.GetXaxis().GetXmax()
                else:
                    return hist.GetNbinsX(), xBins
            elif axis == "y":
                yBins = hist.GetYaxis().GetXbins().GetArray()
                if len(yBins) == 0:
                    return hist.GetNbinsY(), hist.GetYaxis().GetXmin(), hist.GetYaxis().GetXmax()
                else:
                    return hist.GetNbinsY(), yBins
        else:
            raise ValueError("Histogram is not 1D or 2D")

    if len(hists) == 0:
        raise ValueError("Histogram list is empty")

    # Determine if histograms are 1D or 2D
    is2D = isinstance(hists[0], ROOT.TH2)

    if len(hists) == 1:
        return hists[0]

    if is2D:
        # 2D histogram case
        axisParamsX = getAxisParam(hists[0], axis="x")
        axisParamsY = getAxisParam(hists[0], axis="y")
        medianHist = ROOT.TH2F(hists[0].GetName(), "Median " + hists[0].GetTitle(), *axisParamsX, *axisParamsY)
        nBinsX = axisParamsX[0]
        nBinsY = axisParamsY[0]
        for xBin in range(1, nBinsX + 1):
            for yBin in range(1, nBinsY + 1):
                vals = [h.GetBinContent(xBin, yBin) for h in hists]
                medianHist.SetBinContent(xBin, yBin, median(vals))
    else:
        # 1D histogram case
        axisParams = getAxisParam(hists[0])
        medianHist = ROOT.TH1F(hists[0].GetName(), "Median " + hists[0].GetTitle(), *axisParams)
        for xBin in range(1, axisParams[0] + 1):
            vals = [h.GetBinContent(xBin) for h in hists]
            medianHist.SetBinContent(xBin, median(vals))

    return medianHist

def normalize_histogram(hist):
    if isinstance(hist[0], list):  # Check if the histogram is 2D
        # Convert 2D histogram bin contents to a probability distribution
        total = sum(sum(row) for row in hist)  # Sum all elements in the 2D histogram
        return [[h / total for h in row] for row in hist]
    else:
        # Convert 1D histogram bin contents to a probability distribution
        total = sum(hist)  # Sum all elements in the 1D histogram
        return [h / total for h in hist]

def calculate_kl_divergence(P, Q):
    # Flatten 2D histograms if necessary
    if isinstance(P[0], list):
        P = [item for sublist in P for item in sublist]
    if isinstance(Q[0], list):
        Q = [item for sublist in Q for item in sublist]

    # Calculate KL Divergence between two probability distributions P and Q
    return sum(p * math.log(p / q) for p, q in zip(P, Q) if p != 0 and q != 0)

def assign_quality(hists, medianHist):
    # Normalize the median histogram
    Q = normalize_histogram(medianHist)
    
    quality_scores = []
    for hist in hists:
        # Normalize the current histogram
        P = normalize_histogram(hist)
        
        # Calculate KL Divergence
        kl_div = calculate_kl_divergence(P, Q)
        quality_scores.append(kl_div)
        
    # Calculate the mean of the quality scores
    mean_score = sum(quality_scores) / len(quality_scores)

    # Calculate the variance of the quality scores
    variance = sum((score - mean_score) ** 2 for score in quality_scores) / len(quality_scores)

    # Calculate the standard deviation (sigma) of the quality scores
    sigma = math.sqrt(variance)

    quality = []
    for hist in hists:
        Q = normalize_histogram(hist)
        kl_div1 = calculate_kl_divergence(P, Q)
    
        # Assign quality based on KL divergence thresholds
        if abs(kl_div1) < sigma:
            quality.append("good")
        elif abs(kl_div1) < 2*sigma:
            quality.append("medium")
        else:
            quality.append("bad")
    
    return quality

def print_quality_scores(hists, medianHist, percent_diff=.20):
    # Normalize the median histogram
    Q = normalize_histogram(medianHist)
    kl_divergences = []

    # First pass: calculate KL divergences to determine the mean
    for hist in hists:
        P = normalize_histogram(hist)
        kl_div = calculate_kl_divergence(P, Q)
        kl_divergences.append(kl_div)

    # Calculate the mean of KL divergences
    mean_kl_div = sum(kl_divergences) / len(kl_divergences)

    # Calculate the variance of the quality scores
    variance = sum((score - mean_kl_div) ** 2 for score in kl_divergences) / len(kl_divergences)

    # Calculate the standard deviation (sigma) of the quality scores
    sigma = math.sqrt(variance)
   
    # Define thresholds based on mean and percentage difference
    threshold_good = mean_kl_div * (1 - percent_diff)
    threshold_bad = mean_kl_div * (1 + percent_diff)

    # Second pass: categorize histograms based on calculated thresholds
    for i, (hist, kl_div) in enumerate(zip(hists, kl_divergences)):
        if kl_div <= sigma:
            quality = "good"
        elif kl_div <= 2*sigma:
            quality = "medium"
        else:
            quality = "bad"
        print(f"Histogram {i+1}: Quality is {quality} (KL Divergence: {kl_div})")

def main(path, fileList, runList):
    # Set the batch mode to avoid opening windows
    ROOT.gROOT.SetBatch(True)

    # Read the Root Files
    rootDataFile=[]
    for file in fileList:
        rootDataFile.append(ROOT.TFile.Open(file,"READ"))
    
    #Get directories
    directories = [key.GetTitle() for key in rootDataFile[0].GetListOfKeys() if key.GetClassName() == "TDirectoryFile"]

    #Create output file
    outputFileName = f"{path}/periodOverview.root"
    outputfile = ROOT.TFile(outputFileName, "RECREATE")

    medianHists = outputfile.mkdir("medianHists")
    medianHists.Write()  # Write the directory to ensure it's created

    medianHistsSubDirs = [medianHists.mkdir(directory) for directory in directories]
    for subDir in medianHistsSubDirs:
        subDir.Write()  # Write the subdirectory

    trendings = outputfile.mkdir("trendings")
    trendings.Write()  # Write the directory
    trendingsSubDirs = [trendings.mkdir(directory) for directory in directories]
    for subDir in trendingsSubDirs:
        subDir.Write()  # Write the subdirectory
        
    kl_div = outputfile.mkdir("kl_div_trendings")
    kl_div.Write()  # Write the directory
    kl_divSubDirs = [kl_div.mkdir(directory) for directory in directories]
    for subDir in kl_divSubDirs:
        subDir.Write()  # Write the subdirectory

    # Ensure that the output file knows the current directory structure
    outputfile.SaveSelf(True)

    # Create quality DF
    qualityDF = pd.DataFrame({'runNumber': runList})

    # Make trending plots
    print("Generating trending plots")
    trending = "mean"
    error = "meanError"

    for dirID, directory in enumerate(directories):
        objects = [key.GetName() for key in rootDataFile[0].Get(directory).GetListOfKeys() if "TH1" in key.GetClassName()]
        for objectName in objects:
            if "hdEdxTotMIP_" in objectName:
                trending = "fit(gaus,Sq,N,40,60)"
            else:
                trending = "mean"

            # Draw trending
            [trend, canvas] = drawTrending(objectName, rootDataFile, names=runList, namesFromRunList=True, trend=trending, error=error, log="none", axis=1)

            # Check trending and quality, which returns the desired objects
            [qualities, canvas] = checkTrending(trend, canvas=canvas, thresholds={"GOOD": 1.5, "MEDIUM": 3, "BAD": 6})
            qualityDF[objectName+"_trend"] = qualities

            # Navigate to the desired directory
            outputfile.cd(trendingsSubDirs[dirID].GetPath())
            canvas.Write()

    # Create median histogram and kl divergence trending plots
    print("Generating median histograms and KL divergence trending plots")
    for dirID, directory in enumerate(directories):
        # Looking at 1D histograms only for now (2D histograms take too long to process)
        objects = [key.GetName() for key in rootDataFile[0].Get(directory).GetListOfKeys() if "TH" in key.GetClassName()]
        for objectName in objects:
            [hist, legend, canvas, pad1] = drawHistograms(objectName, rootDataFile, normalize=True)
            medianHist = getMedianHistogram(hist)

            if medianHist is None:
                continue  # Skip to the next iteration of the loop

            # Navigate to the desired directory
            outputfile.cd(medianHistsSubDirs[dirID].GetPath())
            medianHist.Write()

            # Draw trending
            [trend, canvas] = drawTrending(objectName, rootDataFile, names=runList, namesFromRunList=True, trend="kl_divergence", error="", axis=1, meanHistogram=medianHist)

            # Check trending and quality, which returns the desired objects
            [qualities, canvas] = checkTrending(trend, canvas=canvas, thresholds={"GOOD": 1.5, "MEDIUM": 3, "BAD": 6})
            qualityDF[objectName+"_klDiv"] = qualities

            # Navigate to the desired directory
            outputfile.cd(kl_divSubDirs[dirID].GetPath())
            canvas.Write()
    
    # Create quality matrix plot
    print("Generating quality matrix plot")
    myPalette = np.array([920, 414, 801, 633],dtype=np.int32)
    ROOT.gStyle.SetPalette(4,myPalette)
    ROOT.gStyle.SetGridStyle(1)
    qualityLabels = {"NULL":0,"GOOD":1,"MEDIUM":2,"BAD":3}

    canvas = ROOT.TCanvas("qualityMatrix","",1000,500)
    canvas.SetLeftMargin(0.15)
    canvas.SetBottomMargin(0.15)
    canvas.SetRightMargin(0.15)
    canvas.SetGrid()

    qualityHist = ROOT.TH2I("qualityMatrix","Quality Matrix",
                            len(qualityDF.index),min(qualityDF.index),max(qualityDF.index)+1,
                            len(qualityDF.columns)-1,0,len(qualityDF.columns)-1)
    qualityHist.SetCanExtend(ROOT.TH1.kAllAxes)
    qualityHist.SetStats(0)
    for runIndex,run in enumerate(qualityDF.runNumber):
        qualityHist.GetXaxis().SetBinLabel(runIndex+1,str(run))
        qualityHist.GetXaxis().SetTickLength( 0.03)
        for checkIndex,check in enumerate(qualityDF.loc[:, qualityDF.columns != "runNumber"].columns):
            #print(checkIndex,runIndex)
            qualityHist.Fill(run,check,qualityLabels.get(qualityDF.iloc[runIndex][check]))

    qualityHist.LabelsOption("u")
    qualityHist.Draw("COLZ")
    qualityHist.GetZaxis().SetRangeUser(-0.5,3.5)
    qualityHist.GetZaxis().SetTitle("Quality")
    qualityHist.LabelsOption("v")

    canvas.Update()
    outputfile.cd()
    canvas.Write()

    # Close the output file
    outputfile.Close()

if __name__ == "__main__":
    # Get the command-line arguments
    parser = argparse.ArgumentParser(description="Script for processing data")
    parser.add_argument("local_dir", help="Path to the local directory")
    args = parser.parse_args()

    fileList = glob.glob(f"{args.local_dir}/*_QC.root")
    fileList = [file for file in fileList if file[-13] != "_"]
    fileList.sort()
    runList = [fileList[i][-14:-8] for i in range(len(fileList))]

    print(f"Running period post processing in batch mode for directory {args.local_dir} with {len(runList)} runs.")
    main(args.local_dir, fileList, runList)
import math
from math import sqrt
import re
from socket import NI_NUMERICHOST
import ROOT
from TPCQCVis.src.utility import *

#calculates KL value
def calculate_kl_divergence(P, Q):
    # Flatten 2D histograms if necessary
    if isinstance(P[0], list):
        P = [item for sublist in P for item in sublist]
    if isinstance(Q[0], list):
        Q = [item for sublist in Q for item in sublist]
    # Calculate KL Divergence between two probability distributions P and Q
    return sum(p * math.log(p / q) for p, q in zip(P, Q) if p != 0 and q != 0)

# Function to draw a trending histogram
def drawTrending(histogram, fileList, files=-1, canvas=[], names=[], debug=False, drawOption="SAME L P E",axis=1, trend="mean", error="stdDev", namesFromRunList=False, log="none",  xAxisRange = [0,0], yAxisRange = [0,0],histName="",meanHistogram=None): 

    # Set the log scale of the canvas
    def logScale(log):
        if log == "none":
            pass
        elif log == "logx":
            ROOT.gPad.SetLogx()
        elif log =="logy":
            ROOT.gPad.SetLogy()
        elif log =="logxy":
            ROOT.gPad.SetLogx()
            ROOT.gPad.SetLogy()
        else:
            raise ValueError("Undefined log called: "+log)
    
    # Extracting the name of the histogram
    if ";" in histogram :
        histogram_name = histogram[0:histogram.find(";")]
    else : histogram_name = histogram

    # Check how many input files are to be processed
    if files == -1 : files = len(fileList)
    if files > len(fileList) : raise ValueError("Number of files to be displayed is larger than files in file list")

    # Creating the canvas object to draw on
    if canvas == [] : 
        canvas = ROOT.TCanvas(histogram_name+"_trend",histogram+"_trend",1100,400)
        canvas.SetBottomMargin(0.15)
        canvas.SetGridx()

    # Trending histogram options
    hTrending = ROOT.TH1F(histogram_name+"_"+trend+"_trend"+histName,
                         histogram_name+"Trending;Run;"+histogram_name+" "+trend,
                         files,0,files)
    hTrending.SetLineWidth(2)
    hTrending.SetMarkerSize(1.5)
    hTrending.SetMarkerStyle(ROOT.kFullCircle)
    hTrending.SetStats(0)
    xValue = 0

    # Looping through each file to extract histogram data
    for i in range(files):
        hist = getHistogram(fileList[i], histogram)
        
        if namesFromRunList: xValue = names[i]
        else : xValue = i
        if debug : print("Adding point "+str(i)+"/"+str(files)+" to trending: "+str(xValue))

        # Filling the trending histogram according to the specified trend option 
        if trend == "mean" :
            hTrending.Fill(xValue,hist.GetMean(axis))
        elif trend == "kl_divergence" :
            hTrending.Fill(xValue,calculate_kl_divergence(hist,meanHistogram))
        elif trend == "entries" :
            hTrending.Fill(xValue,hist.GetEntries())
        elif trend == "stdDev" :
            hTrending.Fill(xValue,hist.GetStdDev(axis))
        elif trend[0:3] == "fit" :
            # Using Root::TH1:Fit("function","fit option","drawing option",fit limit low,fit limit high)
            pattern = "fit\((.*?),(.*?),(.*?),(.*?),(.*?)\)"
            search = re.search(pattern, trend)
            fit = hist.Fit(search.group(1),search.group(2),search.group(3),float(search.group(4)),float(search.group(5)))
            hTrending.Fill(xValue,hist.GetFunction(search.group(1)).GetParameter(1))
        else : raise ValueError("Unknown trend option, please choose mean, entires, stdDev or fit(,,,,)")

        if error == "stdDev" : hTrending.SetBinError(i+1,hist.GetStdDev(axis))
        elif error == "meanError" : hTrending.SetBinError(i+1,(hist.GetStdDev(axis)/sqrt(hist.GetEntries())))
        elif error == "" : hTrending.SetBinError(i+1,0)
        elif error == "const" : hTrending.SetBinError(i+1,100)
        else : raise ValueError("Unknown error option, please choose stdDev or meanError")
    
    # Axis range scaling
        if xAxisRange != [0,0] : 
            hTrending.GetXaxis().SetRangeUser(xAxisRange[0],xAxisRange[1])
        if yAxisRange != [0,0] : 
            hTrending.GetYaxis().SetRangeUser(yAxisRange[0],yAxisRange[1])

    if debug : print("Drawing trending plot")

    if log != "none":
            logScale(log)

    # Drawing the trending histogram
    hTrending.LabelsOption("v")
    hTrending.Draw(drawOption)
    canvas.Update()
    # need to return canvas also otherwise jupyter notebook doesn't display
    return hTrending,canvas
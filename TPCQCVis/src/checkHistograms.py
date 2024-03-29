import ROOT
import math

def checkHistograms(histogram,fileList,files=-1,check="entries>0",condition=10,axis=1,debug=False,printQuality=False):
    result = []
    if check == "" : raise ValueError("Please provide valid check condition.")

    if files == -1 : files = len(fileList)
    if files > len(fileList) : raise ValueError("Number of files to be displayed is larger than files in file list")

    for i in range(files):
        hist = fileList[i].PIDQC.Get(histogram)
        if not hist : hist = fileList[i].TracksQC.Get(histogram)
        if not hist : raise ValueError("Histogram not found "+histogram)

        if debug : print("Checking histogram: "+str(i)+"/"+str(files))

        check_variables = {
            "histogram": hist,
            "mean":    hist.GetMean(axis),
            "entries": hist.GetEntries(),
            "stdDev":  hist.GetStdDev(axis),
            "meanError": hist.GetStdDev(axis)/math.sqrt(hist.GetEntries()),
            "math": math,
            "ROOT": ROOT
            }
            
        if eval(check,check_variables):
            result.append("GOOD")
            if printQuality : print(str(fileList[i]) + ": GOOD")
        else:
            result.append("BAD")
            if printQuality : print(str(fileList[i]) + ": BAD")
    return result
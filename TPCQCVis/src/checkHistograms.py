import ROOT
import math

def checkHistograms(histogram,fileList,files=-1,check="mean",condition=10,axis=1,debug=False):
    result = []
    if files == -1 : files = len(fileList)
    if files > len(fileList) : raise ValueError("Number of files to be displayed is larger than files in file list")

    for i in range(files):
        hist = fileList[i].PIDQC.Get(histogram)
        if not hist : hist = fileList[i].TracksQC.Get(histogram)
        if not hist : raise ValueError("Histogram not found "+histogram)

        if debug : print("Checking histogram: "+str(i)+"/"+str(files))

        check_variables = {
            "mean":    hist.GetMean(axis),
            "entries": hist.GetEntries(),
            "stdDev":  hist.GetStdDev(axis),
            "meanError" : hist.GetStdDev(axis)/math.sqrt(hist.GetEntries())
            }
        if eval(check,check_variables):
            result.append("GOOD")
        else:
            result.append("BAD")
    return result
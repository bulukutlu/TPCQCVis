from math import sqrt
import re
from socket import NI_NUMERICHOST
import ROOT

def drawTrending(histogram, fileList, files=-1, canvas=[], names=[], debug=False, drawOption="SAME L P E PLC PMC",
axis=1, trend="mean", error="stdDev", namesFromRunList=False, log="none",  xAxisRange = [0,0], yAxisRange = [0,0],histName=""): 

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
    
    histogram_name = histogram[0:histogram.find(";")]
    if files == -1 : files = len(fileList)
    if files > len(fileList) : raise ValueError("Number of files to be displayed is larger than files in file list")

    if canvas == [] : canvas = ROOT.TCanvas(histogram_name+"_trend",histogram+"_trend",800,600)

    # Trending histogram options
    hTrending = ROOT.TH1F(histogram_name+"_"+trend+"_trend"+histName,
                         histogram_name+"Trending;Run;"+histogram_name+" "+trend,
                         files,0,files)
    #graph.SetTitle(histogram_name+" Trending;Run;"+histogram_name+" "+trend)
    #graph.SetEditable(False)
    hTrending.SetLineWidth(2)
    hTrending.SetMarkerSize(1.5)
    hTrending.SetMarkerStyle(ROOT.kFullCircle)
    hTrending.SetStats(0)
    xValue = 0
    for i in range(files):
        try:
            if debug : print("Getting histogram: "+histogram)
            hist = fileList[i].PIDQC.Get(histogram)
            if not hist : hist = fileList[i].TracksQC.Get(histogram)
            if not hist : hist = fileList[i].PID.Get(histogram)
            if not hist : hist = fileList[i].Tracks.Get(histogram)
        except: 
            raise ValueError("Histogram not found "+histogram + " test"+ str(i))
        
        if namesFromRunList: xValue = names[i]
        else : xValue = i
        if debug : print("Adding point "+str(i)+"/"+str(files)+" to trending: "+str(xValue))

        if trend == "mean" : hTrending.Fill(xValue,hist.GetMean(axis))
        elif trend == "entries" : hTrending.Fill(xValue,hist.GetEntries())
        elif trend == "stdDev" : hTrending.Fill(xValue,hist.GetStdDev(axis))
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

    hTrending.Draw(drawOption)
    #graph.GetXaxis().SetNdivisions(10)
    canvas.Update()
    return hTrending,canvas
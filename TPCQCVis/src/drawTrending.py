from math import sqrt
import re
from socket import NI_NUMERICHOST
import ROOT

def drawTrending(histogram, fileList, files=-1, canvas=[], names=[], debug=False, drawOption="ZPA PMC L",
axis=1, trend="mean", error="stdDev", namesFromRunList=False, log="none"): 

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

    if canvas == [] : canvas = ROOT.TCanvas(histogram+"_trend",histogram+"_trend",800,600)

    # Trending graph options
    graph = ROOT.TGraphErrors()
    graph.SetTitle(histogram_name+" Trending;Run;"+histogram_name+" "+trend)
    graph.SetEditable(False)
    graph.SetLineWidth(2)
    graph.SetMarkerSize(1.5)
    graph.SetMarkerStyle(21)

    for i in range(files):
        hist = fileList[i].PIDQC.Get(histogram)
        if not hist : hist = fileList[i].TracksQC.Get(histogram)
        if not hist : raise ValueError("Histogram not found "+histogram)

        if debug : print("Adding point to trending: "+str(i)+"/"+str(files))

        if trend == "mean" : graph.SetPoint(i,i+1,hist.GetMean(axis))
        elif trend == "entries" : graph.SetPoint(i,i+1,hist.GetEntries())
        elif trend == "stdDev" : graph.SetPoint(i,i+1,hist.GetStdDev(axis))
        elif trend[0:3] == "fit" :
            # Using Root::TH1:Fit("function","fit option","drawing option",fit limit low,fit limit high)
            pattern = "fit\((.*?),(.*?),(.*?),(.*?),(.*?)\)"
            search = re.search(pattern, trend)
            fit = hist.Fit(search.group(1),search.group(2),search.group(3),float(search.group(4)),float(search.group(5)))
            graph.SetPoint(i,i+1,hist.GetFunction(search.group(1)).GetParameter(1))
        else : raise ValueError("Unknown trend option, please choose mean, entires, stdDev or fit(,,,,)")

        if error == "stdDev" : graph.SetPointError(i,0.5,hist.GetStdDev(axis))
        elif error == "meanError" : graph.SetPointError(i,0.5,(hist.GetStdDev(axis)/sqrt(hist.GetEntries())))
        elif error == "" : graph.SetPointError(i,0.5,0)
        else : raise ValueError("Unknown error option, please choose stdDev or meanError")
    
    if namesFromRunList:
        ax = graph.GetXaxis()
        ax.Set(files+1,0.5,files+1.5)
        x1 = ax.GetBinLowEdge(1)
        x2 = ax.GetBinUpEdge(ax.GetNbins())
        if debug : print("graph:"+str(x1)+" , "+str(x2)+", "+str(ax.GetNbins()))
        for i in range(files) :
            binIndex = ax.FindBin(i+1)
            graph.GetXaxis().SetBinLabel(binIndex,names[i])
            if debug : print(str(binIndex)+" ["+str(ax.GetBinLowEdge(binIndex))+","+str(ax.GetBinUpEdge(binIndex))+", "+str(names[i]))
    
    if debug : print("Drawing trending plot")

    if log != "none":
            logScale(log)

    graph.Draw(drawOption)
    #graph.GetXaxis().SetNdivisions(10)
    canvas.Update()
    return graph,canvas
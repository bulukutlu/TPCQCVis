import math
from statistics import mean
from math import sqrt
import re
from socket import NI_NUMERICHOST
import ROOT

def drawMultiTrending(name, fileList, files=-1, outCanvas=[], names=[], debug=False, drawOption="SAME L P E PLC PMC",
axis=1, trend="mean", error="stdDev", pads=False, normalize=False, namesFromRunList=False, log="none",  xAxisRange = [0,0], yAxisRange = [0,0],histName=""): 

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
    
    def getCanvas(file,name):
        try:
            if debug : print("Getting histogram: "+name)
            canvas = file.PIDQC.Get(name)
            if not canvas : canvas = file.TracksQC.Get(name)
            if not canvas : canvas = file.ClusterQC.Get(name)
            if not canvas : canvas = file.PID.Get(name)
            if not canvas : canvas = file.Tracks.Get(name)
            if not canvas : canvas = file.Cluster.Get(name)
        except: 
            raise ValueError("Histogram not found "+name)
        return canvas
    
    def getHistFromPad(pad):
        # Create a TIter object to iterate over all primitives in the pad
        iter = pad.GetListOfPrimitives().MakeIterator()
        # Loop over all primitives and look for a histogram
        histogram = None
        obj = iter.Next()
        while obj:
            if isinstance(obj, ROOT.TH1):
                histogram = obj
                break  # found a histogram, exit the loop
            obj = iter.Next()

        if not histogram:
            print("No histogram found in the pad: ",iPad)
        return histogram

    name = name[0:name.find(";")]
    if files == -1 : files = len(fileList)
    if files > len(fileList) : raise ValueError("Number of files to be displayed is larger than files in file list")

    if outCanvas == [] : 
        if pads:
            outCanvas = ROOT.TCanvas(name+"_trend",name+"_trend",1000,1000)
        else:
            outCanvas = ROOT.TCanvas(name+"_trend",name+"_trend",1000,600)
            #outCanvas.SetBottomMargin(0.15)
        outCanvas.SetGridx()

    # number of pads in input canvas
    nPads = len(getCanvas(fileList[0],name).GetListOfPrimitives())

    # Draw each trend in one pad if wanted
    if pads:
        outCanvas.Divide(round(math.sqrt(nPads)),math.ceil(math.sqrt(nPads)))
    # Get number of     
    
    # Trending histogram options
    hTrendings = [ROOT.TH1F(name+"_"+trend+"_trend_"+histName+str(nRun),
                         name+"Trending;Run;"+histName+" "+trend,
                         files,0,files) for nRun in range(nPads)]
    
    xValue = 0
    for file in range(files):
        canvas = getCanvas(fileList[file],name)
        
        if namesFromRunList: xValue = names[file]
        else : xValue = file
        
        if normalize :
            normalization = mean([getHistFromPad(pad).GetMean(axis) for pad in canvas.GetListOfPrimitives() if getHistFromPad(pad)])

        # loop over all pads in canvas
        for iPad,pad in enumerate(canvas.GetListOfPrimitives()):
            # get hist from pad
            histTmp = getHistFromPad(pad)
            if histTmp : hist = histTmp #in case no pad is found that it doesn't break
            if not hist : continue

            # trending values
            yValue = 0
            if debug : print("Adding point "+str(file)+"/"+str(files)+" to trending: "+str(xValue))
            if trend == "mean" :
                yValue = hist.GetMean(axis)
            elif trend == "entries" :
                yValue = hist.GetEntries()
            elif trend == "stdDev" :
                yValue = hist.GetStdDev(axis)
            elif trend[0:3] == "fit" :
                # Using Root::TH1:Fit("function","fit option","drawing option",fit limit low,fit limit high)
                pattern = "fit\((.*?),(.*?),(.*?),(.*?),(.*?)\)"
                search = re.search(pattern, trend)
                fit = hist.Fit(search.group(1),search.group(2),search.group(3),float(search.group(4)),float(search.group(5)))
                yValue = hist.GetFunction(search.group(1)).GetParameter(1)
            else : raise ValueError("Unknown trend option, please choose mean, entires, stdDev or fit(,,,,)")
            
            if normalize : yValue /= normalization
            hTrendings[iPad].Fill(xValue,yValue) # fill trending histogram
            
            # error bars
            if error == "stdDev" : hTrendings[iPad].SetBinError(file+1,hist.GetStdDev(axis))
            elif error == "meanError" : hTrendings[iPad].SetBinError(file+1,(hist.GetStdDev(axis)/sqrt(hist.GetEntries())))
            elif error == "" : hTrendings[iPad].SetBinError(file+1,0)
            elif error == "const" : hTrendings[iPad].SetBinError(file+1,100)
            else : raise ValueError("Unknown error option, please choose stdDev or meanError")
    
    for index,hTrending in enumerate(hTrendings):
        # Drawing options
        hTrending.SetLineWidth(2)
        hTrending.SetMarkerSize(1.5)
        hTrending.SetMarkerStyle(ROOT.kFullCircle)
        hTrending.SetStats(0)
        # Axis range scaling
        if xAxisRange != [0,0] : 
            hTrending.GetXaxis().SetRangeUser(xAxisRange[0],xAxisRange[1])
        if yAxisRange != [0,0] : 
            hTrending.GetYaxis().SetRangeUser(yAxisRange[0],yAxisRange[1])
        if debug : print("Drawing trending plot")
        if log != "none":
                logScale(log)
        hTrending.LabelsOption("v")
        if pads: outCanvas.cd(index+1)
        else : outCanvas.cd()
        hTrending.Draw(drawOption)

    outCanvas.Update()
    return hTrendings,outCanvas
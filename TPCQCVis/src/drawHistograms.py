import ROOT
import math
import pandas as pd
from TPCQCVis.src.utility import *


def drawHistograms(histogram, fileList, files=-1, canvas=[], log="none", normalize=False, addHistos=False,
pads=False, legend=False, legendNames=[], debug=False, check=pd.DataFrame(), drawOption="SAME L", pad1=[], xAxisRange = [0,0], yAxisRange = [0,0],
compareTo=None, maxColumns = 6, ratio=True, grid=True,size=None,canvasName=None,histosRatio = None):
    if canvasName is None:
        canvasName = histogram
    def logScale(log):
        if log == "none":
            pass
        elif log == "logx":
            ROOT.gPad.SetLogx()
        elif log =="logy":
            ROOT.gPad.SetLogy()
        elif log =="logz":
            ROOT.gPad.SetLogz()
        elif log =="logxy":
            ROOT.gPad.SetLogx()
            ROOT.gPad.SetLogy()
        elif log =="logxz":
            ROOT.gPad.SetLogx()
            ROOT.gPad.SetLogz()
        elif log =="logyz":
            ROOT.gPad.SetLogy()
            ROOT.gPad.SetLogz()
        elif log =="logxyz":
            ROOT.gPad.SetLogx()
            ROOT.gPad.SetLogy()
            ROOT.gPad.SetLogz()
        else:
            raise ValueError("Undefined log called: "+log)

    if files == -1 : files = len(fileList)
    if files > len(fileList) : raise ValueError("Number of files to be displayed is larger than files in file list")
    if canvas == [] : 
        if pads : 
            if math.ceil(math.sqrt(files)) > maxColumns:
                canvas = ROOT.TCanvas(canvasName,histogram,1000,300*math.ceil(files/maxColumns))
            else:
                if len(fileList) <= 2:
                    canvas = ROOT.TCanvas(canvasName,histogram,1000,450)
                else:
                    canvas = ROOT.TCanvas(canvasName,histogram,1000,800)
        else :
            if not size:
                canvas = ROOT.TCanvas(canvasName,histogram,800,600)
            else:
                canvas = ROOT.TCanvas(canvasName,histogram,size[0],size[1])

    #creates TPad
    pad1 = ROOT.TPad("pad1"+canvasName,"The pad with the content", 0,0,1,1)
    #splits pad
    
    if pads:
        if math.ceil(math.sqrt(files)) <= maxColumns:
            if len(fileList) <= 2:
                pad1.Divide(math.ceil(math.sqrt(files)),round(math.sqrt(files)))
            else:
                pad1.Divide(round(math.sqrt(files)),math.ceil(math.sqrt(files)))
        else:
            pad1.Divide(maxColumns,math.ceil(files/maxColumns))
        
    histos = []
    histosComp = []
    if histosRatio is None:
        histosRatio = []
    leg=[]
    if legend:
        leg = ROOT.TLegend()
        if normalize : leg.SetHeader("Normalized to integral")
        if files > 10 : leg.SetNColumns(2)

    for i in range(files):
        if i==0 or not addHistos :
            hist = getHistogram(fileList[i], histogram)

        if compareTo :
            histComp = getHistogram(compareTo[i], histogram)
            histComp.SetName("Comparison")

        if legend:
            if not check.empty : leg.AddEntry(hist, legendNames[i]+" (Quality::"+check[i]+")", "l")              
            else : leg.AddEntry(hist, legendNames[i], "l")

        if normalize:
            hist.Scale(1/hist.Integral())
            if compareTo : histComp.Scale(1/histComp.Integral())

        if addHistos:
            if i != 0:
                hist2 = getHistogram(fileList[i], histogram)
                hist.Add(hist2)
        
        if log != "none":
            if not pads :
                pad1.cd()
            logScale(log)

        if not compareTo : hist.SetLineWidth(3)
        if pads : hist.SetLineColor(1)
        else : hist.SetLineColor(i+1)

        if len(check):
            # Make histograms filled greed/red depending on quality
            hist.SetFillStyle(3001)
            if check[i] == "GOOD" : hist.SetFillColorAlpha(ROOT.kGreen,0.5)
            elif check[i] == "MEDIUM" : hist.SetFillColorAlpha(ROOT.kOrange,0.5)
            elif check[i] == "BAD" : hist.SetFillColorAlpha(ROOT.kRed,0.5)

        if pads and legendNames : hist.SetTitle(legendNames[i]+" - "+hist.GetTitle())
        if legendNames : hist.SetName(legendNames[i])

        #Create ratio plots when comparing
        if compareTo :
            hist.SetLineColor(ROOT.kBlue+3)
            hist.SetMarkerColor(ROOT.kBlue+3)
            hist.SetLineWidth(2)
            histComp.SetLineColor(ROOT.kRed-3)
            histComp.SetMarkerColor(ROOT.kRed-3)
            histComp.SetLineWidth(2)
            histComp.SetTitle(hist.GetTitle())
            histosComp.append(histComp)
            if ratio:
                if type(hist) in (ROOT.TH1D, ROOT.TH1F, ROOT.TH1C, ROOT.TH2D, ROOT.TH2F, ROOT.TH2C):
                    histRatio = hist.Clone("hRatio_"+str(i))
                    histRatio.Divide(histComp)
                    histRatio.SetTitle("Ratio")
                    histRatio.SetStats(0)
                    histRatio.SetLineColor(1)
                    histRatio.SetLineWidth(2)
                    histosRatio.append(histRatio)
                else:
                    raise TypeError("Histograms should be TH1 or TH2")
            
        # Axis range scaling
        if xAxisRange != [0,0] : 
            hist.GetXaxis().SetRangeUser(xAxisRange[0],xAxisRange[1])
            if compareTo : 
                histComp.GetXaxis().SetRangeUser(xAxisRange[0],xAxisRange[1])
                if ratio: histRatio.GetXaxis().SetRangeUser(xAxisRange[0],xAxisRange[1])
        if yAxisRange != [0,0] : 
            hist.GetYaxis().SetRangeUser(yAxisRange[0],yAxisRange[1])
            if compareTo :
                histComp.GetYaxis().SetRangeUser(yAxisRange[0],yAxisRange[1])
                if ratio and type(hist) in (ROOT.TH2D, ROOT.TH2F, ROOT.TH2C):
                    histRatio.GetYaxis().SetRangeUser(yAxisRange[0],yAxisRange[1])

        histos.append(hist)
    
        if debug : print("Drawing histogram: "+str(i)+"/"+str(files))
        if not pads:
            pad1.cd()
            hist.Draw(drawOption)
    
    #fills pad with histogram from each file
    if pads:
        for i in range(len(histos)):
            currentPad = pad1.cd(i+1)
            currentPad.SetBorderMode(1)
            if compareTo:
                if type(histos[i]) in (ROOT.TH2D, ROOT.TH2F, ROOT.TH2C):
                    if ratio:
                        currentPad.Divide(3)
                    else:
                        currentPad.Divide(2)
                    currentPad.cd(1)
                    if log != "none" : logScale(log)
                    if grid:
                        ROOT.gPad.SetGridx(1)
                        ROOT.gPad.SetGridy(1)
                    histos[i].Draw(drawOption)
                    currentPad.cd(2)
                    if log != "none" : logScale(log)
                    if grid:
                        ROOT.gPad.SetGridx(1)
                        ROOT.gPad.SetGridy(1)
                    histosComp[i].Draw(drawOption)
                    if ratio: 
                        currentPad.cd(3)
                        if log != "none" : logScale(log)
                        if grid:
                            ROOT.gPad.SetGridx(1)
                            ROOT.gPad.SetGridy(1)
                        histosRatio[i].Draw(drawOption)
                else:
                    if ratio: 
                        currentPad.Divide(1,2)
                        currentPad.cd(1)
                        ROOT.gPad.SetPad(0.05,0.35,0.95,0.95)
                    if log != "none" : logScale(log)
                    if grid:
                        ROOT.gPad.SetGridx(1)
                        ROOT.gPad.SetGridy(1)
                    histosComp[i].Draw(drawOption)
                    histos[i].Draw(drawOption)         
                    if ratio: 
                        currentPad.cd(2)
                        ROOT.gPad.SetPad(0.05,0.05,0.95,0.35)
                        ROOT.gPad.SetTopMargin(0)
                        if log != "none" : logScale(log)
                        if grid:
                            ROOT.gPad.SetGridx(1)
                            ROOT.gPad.SetGridy(1)
                        histosRatio[i].Draw(drawOption)   
            else:
                if log != "none" : logScale(log)
                if grid:
                        ROOT.gPad.SetGridx(1)
                        ROOT.gPad.SetGridy(1)
                histos[i].Draw(drawOption)           
                       
    canvas.cd()
    pad1.Draw(drawOption)

    if legend: leg.Draw()
            
    if compareTo and ratio : return histos,leg,canvas,pad1,histosComp,histosRatio
    elif compareTo : return histos,leg,canvas,pad1,histosComp
    else : return histos,leg,canvas,pad1

import ROOT
import math
import statistics

def checkTrending(trend,check="mean",canvas=[],debug=False,printQuality=False,thresholds={"GOOD":3,"MEDIUM":6,"BAD":9}):
    qualities= []
    
    if check not in ["mean"] : raise ValueError("Please provide valid check condition.")
    nBins = trend.GetNbinsX()
    vals = [trend.GetBinContent(bin+1) for bin in range(nBins)]
    mean = statistics.mean(vals)
    meanError = statistics.stdev(vals)
    if debug : print("Mean",mean,"MeanError",meanError) 
    qualities = []

    if canvas == [] : canvas = ROOT.TCanvas(trend.GetName()+"_check",trend.GetName()+"_check",800,600)

    hGOOD = ROOT.TH1F("hGOOD","",nBins,0,nBins)
    hMEDIUM = ROOT.TH1F("hMEDIUM","",nBins,0,nBins)
    hBAD = ROOT.TH1F("hBAD","",nBins,0,nBins)

    for bin in range(nBins):
        #Adding check bands
        hGOOD.Fill(trend.GetXaxis().GetBinLabel(bin+1),mean)
        hGOOD.SetBinError(bin+1,thresholds.get("GOOD")*abs(trend.GetBinError(bin+1)+meanError))
        hMEDIUM.Fill(trend.GetXaxis().GetBinLabel(bin+1),mean)
        hMEDIUM.SetBinError(bin+1,thresholds.get("MEDIUM")*abs(trend.GetBinError(bin+1)+meanError))
        hBAD.Fill(trend.GetXaxis().GetBinLabel(bin+1),mean)
        hBAD.SetBinError(bin+1,thresholds.get("BAD")*abs(trend.GetBinError(bin+1)+meanError))
        
        if abs(trend.GetBinContent(bin+1)-mean) < thresholds.get("GOOD")*abs(trend.GetBinError(bin+1)+meanError):
            qualities.append("GOOD")
            if printQuality : print(str(trend.GetXaxis().GetBinLabel(bin+1)) + ": GOOD")
        elif abs(trend.GetBinContent(bin+1)-mean) < thresholds.get("MEDIUM")*abs(trend.GetBinError(bin+1)+meanError):
            qualities.append("MEDIUM")
            if printQuality : print(str(trend.GetXaxis().GetBinLabel(bin+1)) + ": MEDIUM")
        else:
            qualities.append("BAD")
            if printQuality : print(str(trend.GetXaxis().GetBinLabel(bin+1)) + ": LABEL")

    #Draw check bands
    canvas.cd()
    hBAD.SetFillColor(ROOT.kRed);
    hBAD.SetFillStyle(3002);
    hBAD.DrawCopy("SAME E2");
    hMEDIUM.SetFillColor(ROOT.kOrange);
    hMEDIUM.SetFillStyle(3002);
    hMEDIUM.DrawCopy("SAME E2");
    hGOOD.SetFillColor(ROOT.kGreen);
    hGOOD.SetFillStyle(3002);
    hGOOD.DrawCopy("SAME E2");
    trend.GetYaxis().SetRangeUser(min(mean-12*abs(trend.GetBinError(bin+1)+meanError),min(vals)-abs(trend.GetBinError(bin+1)+meanError)),
                                max(mean+12*abs(trend.GetBinError(bin+1)+meanError),max(vals)+abs(trend.GetBinError(bin+1)+meanError)));
    trend.SetLineColor(ROOT.kBlack)
    trend.SetMarkerColor(ROOT.kBlack)
    trend.DrawCopy("SAME L P E")
    
    #canvas.Update()
    return qualities, canvas



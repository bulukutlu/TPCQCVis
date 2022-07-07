import ROOT
import math

def drawHistograms(histogram,fileList,files=-1,canvas=[],log="none",normalize=False,addHistos=False,
pads=False,legend=False,legendNames=[],debug=False, drawOption="SAME HIST",pad1=[]):
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

    if canvas == [] : canvas = ROOT.TCanvas(histogram,histogram,800,600)

    #creates TPad
    pad1 = ROOT.TPad("pad1","The pad with the content", 0,0,1,.9)
    #splits pad
    if pads:
        pad1.Divide(round(math.sqrt(files)),round(math.sqrt(files)))
        
    histos = []

    leg=[]
    if legend:
        leg = ROOT.TLegend()
        if normalize : leg.SetHeader("Normalized to integral")

    for i in range(files):
        if i==0 or not addHistos :
            hist = fileList[i].PIDQC.Get(histogram)
        if not hist : hist = fileList[i].TracksQC.Get(histogram)
        if not hist : raise ValueError("Histogram not found "+histogram)

        if legend:
            leg.AddEntry(hist, legendNames[i], "l")

        if normalize:
            hist.Scale(1/hist.Integral())

        if addHistos:
            if i != 0:
                hist2 = fileList[i].PIDQC.Get(histogram)
                if not hist2 : hist2 = fileList[i].TracksQC.Get(histogram)
                if not hist or not hist2 : raise ValueError("[addHistos] Histogram not found "+histogram)
                hist.Add(hist2)
        
        
        if log != "none":
            logScale(log)

        hist.SetLineWidth(3)
        hist.SetLineColor(i+1)
        hist.SetTitle(histogram)
        
        histos.append(hist)
    
        if debug : print("Drawing histogram: "+str(i)+"/"+str(files))
        
        if not pads:
            pad1.cd()
            hist.Draw(drawOption)

    if legend: leg.Draw()
    
    #fills pad with histogram from each file
    if pads:
        for i in range (len(histos)):
            pad1.cd(i+1)
            histos[i].Draw()
            
    canvas.cd()
    pad1.Draw()
            
    return hist,leg,canvas,pad1

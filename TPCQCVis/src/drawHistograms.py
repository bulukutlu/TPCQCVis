import ROOT

def drawHistograms(histogram,fileList,files=-1,canvas=[],log="none",normalize=False,addHistos=False,
pads=False,legend=False,legendNames=[],debug=False, drawOption="SAME HIST"): 
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

    # Split the canvas   
    if pads:
        canvas.Divide(files,files)

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
        
        if pads:
            canvas.cd(i+1)
        
        if log != "none":
            logScale(log)

        hist.SetLineWidth(3)
        hist.SetLineColor(i+1)
        hist.SetTitle(histogram)
    
        if debug : print("Drawing histogram: "+str(i)+"/"+str(files))
        hist.Draw(drawOption)
        canvas.Update()

    if legend : leg.Draw()         
    return hist,leg,canvas
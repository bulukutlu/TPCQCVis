def checkIfExists(files, title):
    objectAvailable = False
    hist = None
    if type(files) is list:
        file = files[0]
    else:
        file = files
    try:
        hist = file.PIDQC.Get(title)
        if not hist : hist = file.TracksQC.Get(title)
        if not hist : hist = file.ClusterQC.Get(title)
        if not hist : hist = file.PID.Get(title)
        if not hist : hist = file.Tracks.Get(title)
    except:
        objectAvailable = False
    else:
        if hist:
            objectAvailable = True
        else:
            objectAvailable = False
    if not objectAvailable:
        print(title,"not found")
    return objectAvailable

  
def sliceAndFit(objectName,rootDataFile,fitFunc="gaus",fitRange=[40,60]):
    from copy import copy
    canvas = ROOT.TCanvas()
    legend = ROOT.TLegend()
    [hist,leg,canvo,pad1] = drawHistograms(objectName,rootDataFile,normalize=False,legend=False,
                                           pads=True,drawOption="COLZ",maxColumns=3)
    fits = []
    myFunc = ROOT.TF1("myFunc","gaus",40,60)
    canvas.cd()
    for i,histo in enumerate(hist):
        histo.FitSlicesY(myFunc)
        fit = copy(ROOT.gDirectory.Get(histo.GetName()+"_1"))
        fit.SetYTitle("Gaus Fit Mean "+ histo.GetYaxis().GetTitle())
        fit.SetXTitle(histo.GetXaxis().GetTitle())
        fit.SetTitle(quant.GetYaxis().GetTitle()+" vs. "+quant.GetXaxis().GetTitle())
        fit.SetLineWidth(2)
        fits.append(fit)
        #legend.AddEntry(fit,runList[i])
        #fit.Draw("SAME L")
    #legend.Draw()
    return fits, legend, canvas
    
def quantileProfile(hist,quantileOrder=0.5, axis="x"):
    from copy import copy
    quants = []
    for i,histo in enumerate(hist):
        if axis == "y":
            quant = copy(histo.QuantilesY(quantileOrder))
        else:
            quant = copy(histo.QuantilesX(quantileOrder))
        quant.SetYTitle("Median "+ histo.GetYaxis().GetTitle())
        quant.SetXTitle(histo.GetXaxis().GetTitle())
        quant.SetTitle(quant.GetYaxis().GetTitle()+" vs. "+quant.GetXaxis().GetTitle())
        quant.SetLineWidth(2)
        quants.append(quant)
        quants[i].SetStats(0)
    return quants
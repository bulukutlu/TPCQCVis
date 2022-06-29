import ROOT

rootDataFile=[]

def DrawDistributions(histogram,file_list,files,a=1,log="none"):
    
    def logscale(log):
        if log == "logx":
            ROOT.gPad.SetLogx()
        if log =="logy":
            ROOT.gPad.SetLogy()
        if log =="logz":
            ROOT.gPad.SetLogz()
        if log =="logxy":
            ROOT.gPad.SetLogx()
            ROOT.gPad.SetLogy()
        if log =="logxz":
            ROOT.gPad.SetLogx()
            ROOT.gPad.SetLogz()
        if log =="logyz":
            ROOT.gPad.SetLogy()
            ROOT.gPad.SetLogz()
                
    if (a==1):
        for i in range(files):
            hist = file_list[i].PID.Get(histogram)
            if not hist:
                hist = file_list[i].Tracks.Get(histogram)
            hist.Scale(1/hist.Integral())
            hist.SetLineWidth(3)
            hist.SetLineColor(i+1)
            hist.SetTitle(histogram)
            hist.Draw("SAME HIST COLZ")
            logscale(log)
        
    if (a==2):
        for i in range(files):
            if i == 0:
                hist = file_list[i].PID.Get(histogram)
                if not hist:
                    hist = file_list[i].Tracks.Get(histogram)
            else:
                histo = file_list[i].PID.Get(histogram)
                if not histo:
                    histo = file_list[i].Tracks.Get(histogram)
                    hist.Add(histo)
                hist.Add(histo)
                logscale(log)
        hist.Draw("SAME COLZ")

    if (a==3):
        for i in range(files):
            c.cd(i+1)
            hist = file_list[i].PID.Get(histogram)
            if not hist:
                hist = file_list[i].Tracks.Get(histogram)
            hist.Draw("COLZ")
            hist.SetTitle(file_list[i][-18:-5])
            logscale(log)

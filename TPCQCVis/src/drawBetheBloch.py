import math
from array import array
import pandas as pd
import numpy as np
import ROOT
import TPCQCVis
import inspect

def getBetheBloch(runList):
    def BetheBlochAleph(bg,kp1=0.820172e-1,kp2=9.94795,kp3=8.97292e-05,kp4=2.05873,kp5=1.65272):
        beta = bg / math.sqrt(1 + bg * bg)
        aa = beta**kp4
        bb = bg**(-kp5)
        bb = math.log(kp3 + bb)
        return (kp2 - aa - bb) * kp1 / aa

    def betaGamma(p,particle):
        mass = {
            "electron" : 0.000511,
            "muon" : 0.105658,
            "pion" : 0.139570,
            "kaon" : 0.493677,
            "proton" : 0.938272,
            "deuteron" : 1.8756129,
            "triton": 2.8089211
        }
        return p/(mass.get(particle))

    def getParams(runNumber):
        pathTPCQCVis = TPCQCVis.__file__[0:-20]
        params_df = pd.read_csv(pathTPCQCVis+"/data/params/params.csv")
        for index,row in params_df.iterrows():
            if row.minRunNumber <= runNumber <= row.maxRunNumber:
                return [row.kp1,row.kp2,row.kp3,row.kp4,row.kp5]
            
    n = 1000
    betheBlochLines = []
    for run in runList:
        betheBlochParams = getParams(int(run))
        lines = []
        particles = ["electron","muon","pion","kaon","proton","deuteron","triton"]
        for particle in particles:
            x, y = array('d'), array('d')
            for i in np.linspace(math.log(5e-2), math.log(20), num=n):  
                p = math.exp(i) 
                x.append( p )
                y.append(50*BetheBlochAleph(betaGamma(p,particle),*betheBlochParams))
            g = ROOT.TGraph( n, x, y )
            g.SetName(particle)
            lines.append(g)
        betheBlochLines.append(lines)

    return betheBlochLines

def drawBetheBloch(betheBlochLines,canvas):
    for pad in range(len(betheBlochLines)):
        canvas.cd(pad+1)
        for line in betheBlochLines[pad]:
            line.Draw("SAME C")
    canvas.Update()
    return canvas
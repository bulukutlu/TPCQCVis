def listTHnOptions(thn):
    axes=[]
    for i in range(thn.GetNdimensions()):
        axes.append(thn.GetAxis(i).GetTitle())
    print(axes)
    
def drawTHnProjection(thn,axis):
    dimensions = []
    for i in range(thn.GetNdimensions()):
        dimensions.append(thn.GetAxis(i).GetTitle())
    if(len(axis) == 1):
        h = thn.Projection(dimensions.index(axis[0]))
        h.GetYaxis().SetTitle('Counts');
        h.Draw()
    elif(len(axis) == 2):
        h = thn.Projection(dimensions.index(axis[1]),dimensions.index(axis[0]))
        h.GetZaxis().SetTitle('Counts');
        h.Draw("colz")
    elif(len(axis) == 3):
        h = thn.Projection(dimensions.index(axis[2]),dimensions.index(axis[1]),dimensions.index(axis[0]))
        h.Draw()
    else: print("Wrong number of dimensions given, please make 1D,2D or 3D projections")
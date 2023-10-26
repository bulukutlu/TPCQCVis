import ROOT
import numpy as np
from sklearn.cluster import DBSCAN
from collections import Counter
from array import array
from scipy.spatial import ConvexHull
from copy import copy

def find_all_hists(pad):
    hists = []
    for prim in pad.GetListOfPrimitives():
        if isinstance(prim, ROOT.TH2):
            hists.append(prim)
        elif isinstance(prim, ROOT.TPad):  # if it's a pad, look inside it
            hists.extend(find_all_hists(prim))
    return hists

def get_bin_center_coordinates(histogram, clustered_bins, labels, cluster_id):
    cluster_bins = np.array([coord for coord, label in zip(clustered_bins, labels) if label == cluster_id])
    if len(cluster_bins) == 0:
        return None
    x_coords = [histogram.GetXaxis().GetBinCenter(int(bin_coord[0])) for bin_coord in cluster_bins]
    y_coords = [histogram.GetYaxis().GetBinCenter(int(bin_coord[1])) for bin_coord in cluster_bins]
    return [np.mean(x_coords), np.mean(y_coords)]

def get_bin_coordinates(histogram, clustered_bins, labels, cluster_id):
    cluster_bins = np.array([coord for coord, label in zip(clustered_bins, labels) if label == cluster_id])
    if len(cluster_bins) == 0:
        return None
    coords = [[histogram.GetXaxis().GetBinCenter(int(bin_coord[0])),
               histogram.GetYaxis().GetBinCenter(int(bin_coord[1]))] for bin_coord in cluster_bins]
    
    return np.array(coords)

def find_clusters_and_report_centers(histogram, bins, min_cluster_size, epsvalue,min_samplesvalue):
    if len(bins) == 0:
        return 0
    db = DBSCAN(eps=epsvalue, min_samples=min_samplesvalue).fit(bins)
    cluster_counts = Counter(db.labels_)
    large_clusters = [cluster for cluster, count in cluster_counts.items() if count > min_cluster_size and cluster != -1]

    #print(f"Number of clusters: {len(large_clusters)}")
    center_coordinates = []
    coords = []
    for cluster_id in large_clusters:
        center = get_bin_center_coordinates(histogram, bins, db.labels_, cluster_id)
        if center is not None:
            print(f"--> Outlier region (centered at x={center[0]:.1f}cm, y={center[1]:.1f}cm) detected in {histogram.GetTitle()}")
            center_coordinates.append(center)
            coords.append(get_bin_coordinates(histogram, bins, db.labels_, cluster_id))

    return len(large_clusters), center_coordinates, coords


def analyze_histogram_from_canvas(canvas,epsvalue=1,min_samplesvalue=5, avvalue=0.75):
    histograms = find_all_hists(canvas)
    x0, y0 = 165, 165
    r_outer = 150
    r_inner = 60

    for histogram in histograms:
        print(f"\nAnalyzing histogram: {histogram.GetName()}")

        nBinsX = histogram.GetNbinsX()
        nBinsY = histogram.GetNbinsY()

        emptyBins = []
        for i in range(1, nBinsX+1):
            for j in range(1, nBinsY+1):
                if histogram.GetBinContent(i, j) == 0:
                    if (i - x0)**2 + (j - y0)**2 <= r_outer**2 and (i - x0)**2 + (j - y0)**2 > r_inner**2:
                        emptyBins.append([i, j])

        print("Finding clusters for empty bins...")
        empty_clusters = find_clusters_and_report_centers(histogram, emptyBins, 50, epsvalue,min_samplesvalue)

        non_empty_bins = []
        for i in range(1, nBinsX+1):
            for j in range(1, nBinsY+1):
                bin_value = histogram.GetBinContent(i, j)
                if bin_value > 0:
                    non_empty_bins.append(bin_value)

        average_value = np.mean(non_empty_bins)

        significant_diff_bins = []
        for i in range(1, nBinsX+1):
            for j in range(1, nBinsY+1):
                bin_value = histogram.GetBinContent(i, j)
                if bin_value > (avvalue * average_value) + average_value or bin_value < average_value - (avvalue * average_value):
                    if (i - x0)**2 + (j - y0)**2 <= r_outer**2 and (i - x0)**2 + (j - y0)**2 > r_inner**2:
                        significant_diff_bins.append([i, j])

        print("Finding clusters for significantly different value bins...")
        diff_clusters = find_clusters_and_report_centers(histogram, significant_diff_bins, 30 ,epsvalue,min_samplesvalue)

        # Evaluate histogram
        if empty_clusters == 0 and diff_clusters == 0:
            print("\n*** GOOD ***\n")
        elif empty_clusters > 0:
            print("\n*** BAD ***\n")
        elif empty_clusters == 0 and diff_clusters > 0:
            print("\n*** MEDIUM ***\n")

    if not histograms:
        print("No 2D histograms found on the canvas.")

def detectOutlierRegions(canvas,epsvalue=1,min_samplesvalue=5, avvalue=0.75):
    histograms = find_all_hists(canvas)
    x0, y0 = 165, 165
    r_outer = 150
    r_inner = 60
    
    outliers_full = []
    centers_full = []
    hulls_full = []
    
    for histogram in histograms:
        nBinsX = histogram.GetNbinsX()
        nBinsY = histogram.GetNbinsY()
        non_empty_bins = []
        for i in range(1, nBinsX+1):
            for j in range(1, nBinsY+1):
                bin_value = histogram.GetBinContent(i, j)
                if bin_value > 0:
                    non_empty_bins.append(bin_value)

        average_value = np.mean(non_empty_bins)

        significant_diff_bins = []
        for i in range(1, nBinsX+1):
            for j in range(1, nBinsY+1):
                bin_value = histogram.GetBinContent(i, j)
                if bin_value > (avvalue * average_value) + average_value or bin_value < average_value - (avvalue * average_value):
                    if (i - x0)**2 + (j - y0)**2 <= r_outer**2 and (i - x0)**2 + (j - y0)**2 > r_inner**2:
                        significant_diff_bins.append([i, j])

        #print("Finding clusters for significantly different value bins...")
        nClusters, centers, coords_list = find_clusters_and_report_centers(histogram, significant_diff_bins, 30 ,epsvalue,min_samplesvalue)
        hulls = []
        if nClusters:
            for cluster in range(nClusters):
                coords = coords_list[cluster]

                # Create convex hull surronding the region
                hull = ConvexHull(coords)
                n = len(hull.vertices)
                x, y = array( 'd' ), array( 'd' )
                for i in range(n):
                    x.append( coords[hull.vertices][i,0] )
                    y.append( coords[hull.vertices][i,1] )
                gHull = ROOT.TGraph(n,x,y)
                gHull.SetTitle("Outlier_region")
                gHull.SetLineColor( 2 )
                gHull.SetLineWidth( 4 )
                gHull.SetMarkerColor( 4 )
                gHull.SetMarkerStyle( 21 )
                gHull.SetFillColorAlpha(2,0.5)
                hulls.append(copy(gHull))

            outliers_full.append(nClusters)
            centers_full.append(centers)
            hulls_full.append(hulls)
        else:
            outliers_full.append(0)
            centers_full.append(None)
            hulls_full.append(None)
            
    if not histograms:
        print("No 2D histograms found on the canvas.")
    else:
        # Draw outlier boundaries on pads
        pads = canvas.GetListOfPrimitives()
        for i in range(2):
            pad = pads[i]
            pad.cd()
            if outliers_full[i]:
                for hull in hulls_full[i]:
                    hull.Draw("SAME L")
        
    return outliers_full,centers_full,hulls_full
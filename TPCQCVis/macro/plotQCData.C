#if !defined(__CLING__) || defined(__ROOTCLING__)
#include <fmt/format.h>
#include "TFile.h"
#include "TCanvas.h"
#include <TH1.h>
#include <TH2.h>
#include "TPCBase/Painter.h"
#include "TPCBase/Utils.h"
#include "TPC/ClustersData.h"
#include "QualityControl/MonitorObject.h"
#include "CommonUtils/StringUtils.h"
#endif

/// This can read a file containing Clusters, PID and Tracks
/// QC output as stored from MC productions.
/// Call the macro with root -b -q plotQCData.C+'("filename.root")'
/// The output will be a file called filename_QC.root containing all QC plots.

void plotQCData(const std::string filename)
{
  /// set up I/O
  TFile *f = new TFile(filename.c_str(), "read");
  auto name = o2::utils::Str::tokenize(filename, '.');
  TFile *fout = new TFile(fmt::format("{}_QC.root", name[0]).data(), "recreate");
   
//-------------------------------------------------

  /// Cluster QC
  TObjArray* clusArr;
  if (f->GetDirectory("TPC")) {
    clusArr = (TObjArray*)f->Get("TPC/Clusters");
  }
  else{
    clusArr = (TObjArray*)f->Get("Clusters");
  }

  if (clusArr) {
    auto mo = (o2::quality_control::core::MonitorObject*)clusArr->At(0);
    auto cl = (o2::quality_control_modules::tpc::ClustersData*)mo->getObject();
  
    fout->cd();
    gDirectory->mkdir("ClusterQC");
    fout->cd("ClusterQC");
  
    /// ----------------------------> YOU CAN SET THE HISTO RANGES HERE!! <-----------------------------
    auto nCl = o2::tpc::painter::draw(cl->getClusters().getNClusters(), 300, 0,cl->getClusters().getNClusters().getMean()*5);       // <-----
    auto qMax = o2::tpc::painter::draw(cl->getClusters().getQMax(), 300, 0, 200);              // <-----
    auto qTot = o2::tpc::painter::draw(cl->getClusters().getQTot(), 300, 0, 600);              // <-----
    auto sigmaTime = o2::tpc::painter::draw(cl->getClusters().getSigmaTime(), 300, 0, 1.); // <-----
    auto sigmaPad = o2::tpc::painter::draw(cl->getClusters().getSigmaPad(), 300, 0, 0.8);   // <-----
    auto timeBin = o2::tpc::painter::draw(cl->getClusters().getTimeBin(), 300, 27500, 29000);  // <-----
  
    nCl->Write("",TObject::kOverwrite);
    qMax->Write("",TObject::kOverwrite);
    qTot->Write("",TObject::kOverwrite);
    sigmaTime->Write("",TObject::kOverwrite);
    sigmaPad->Write("",TObject::kOverwrite);
    timeBin->Write("",TObject::kOverwrite);
  }
//-------------------------------------------------


  /// PID QC
  TObjArray* pidArr;
  if (f->GetDirectory("TPC")) {
    pidArr = (TObjArray*)f->Get("TPC/PID");
  }
  else{
    pidArr = (TObjArray*)f->Get("PID");
  }

  if (pidArr) {
    fout->cd();
    gDirectory->mkdir("PIDQC");
    fout->cd("PIDQC");
  
    for (int i = 0; i < pidArr->GetEntries(); i++) {
      auto pidMO = (o2::quality_control::core::MonitorObject*)pidArr->At(i);
      auto hist = (TH1F*)pidMO->getObject();
      hist->Write("",TObject::kOverwrite);
    }
  }
//-------------------------------------------------


  /// Tracks QC
  TObjArray* trArr;
  if (f->GetDirectory("TPC")) {
    trArr = (TObjArray*)f->Get("TPC/Tracks");
  }
  else{
    trArr = (TObjArray*)f->Get("Tracks");
  }

  if (trArr) {
    fout->cd();
    gDirectory->mkdir("TracksQC");
    fout->cd("TracksQC");
  
    for (int i = 0; i < trArr->GetEntries(); i++) {
      auto trMO = (o2::quality_control::core::MonitorObject*)trArr->At(i);
      auto hist = (TH1F*)trMO->getObject();
      hist->Write("",TObject::kOverwrite);
    }
  }
//-------------------------------------------------

  fout->Close();

  return;
}
#if !defined(__CLING__) || defined(__ROOTCLING__)
#include <fmt/format.h>
#include "TFile.h"
#include "TCanvas.h"
#include "TPCBase/Painter.h"
#include "TPCBase/Utils.h"
#include "CommonUtils/StringUtils.h"
#include "TPCBase/CalDet.h"
#endif

struct binning {
  int nbins;
  float xmin;
  float xmax;
};

const std::vector<binning> bins {
  {300, 114, 116},
  {300, 0, 0},
  {300, 0, 0}
};

void plotCalDetMap(const std::string filename)
{
  TFile *f = new TFile(filename.c_str(), "read");
  auto name = o2::utils::Str::tokenize(filename, '.');
  std::unordered_map<string,o2::tpc::CalDet<float> >* calDetMap;
  f->GetObject("ccdb_object", calDetMap);

  TFile *fout = new TFile(fmt::format("{}_plots.root", name[0]).data(), "recreate");

  int counter = 0;
  for (auto& pair : *calDetMap) {
    std::cout << "Name in map: " << pair.first << ",\t" << "name of calDet object: " << pair.second.getName() << std::endl;
    fout->cd();
    gDirectory->mkdir(pair.first.c_str());
    fout->cd(pair.first.c_str());
    for (auto& canv : o2::tpc::painter::makeSummaryCanvases(pair.second, bins[counter].nbins, bins[counter].xmin, bins[counter].xmax)) {
      canv->Write("", TObject::kOverwrite);
    }
    counter++;
  }

  fout->Close();
}

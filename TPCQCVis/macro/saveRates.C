#include "fmt/format.h"
#include "CCDB/CcdbApi.h"
#include "CCDB/BasicCCDBManager.h"
#include "DataFormatsCTP/Scalers.h"
#include "DataFormatsParameters/GRPECSObject.h"
#include "DataFormatsParameters/GRPLHCIFData.h"
#include "CommonConstants/LHCConstants.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TH1.h"
#include "TLatex.h"
#include <algorithm>
#include "TStyle.h"
#include <numeric>

double getScalerRate(const o2::ctp::CTPScalerRecordO2& first, const o2::ctp::CTPScalerRecordO2& second, int classindex, int type);
//TGraph* getScalerGraph(int run, int timeType = 0, int classindex = -1, int type = -1, double factor = -1);

// Pb-Pb: classindex=25 (ZDC), type=7
// pp: classindex=0 (FT0), type=1
// automatic: classindex = -1, type = -1
// factor PbPb23 ZDC -> FT0 pp: 2.414
//               FT0 -> FT0 pp: 135.
// timeType: 0 = date; 1 = seconds since start of run; 2 = minutes since start of run

/*
void DrawScalers(int run, int timeType = 0, int classindex = -1, int type = -1, double factor = -1)
{
  auto gr = getScalerGraph(run, timeType, classindex, type, factor);

  gStyle->SetTimeOffset(0);

  auto c = new TCanvas;
  if (timeType == 0) {
    gr->Draw("ap");
  } else {
    gr->Draw("alp");
  }

  TLatex l;
  l.SetTextAlign(12);
  l.DrawLatexNDC(gPad->GetLeftMargin(), 1 - gPad->GetTopMargin() / 2, fmt::format("Run {}", run).data());
}
*/

void saveRates(int run, int timeType = 0, int classindex = -1, int type = -1, double factor = -1)
{
  // Get CCDB objects
  auto& ccdbmgr = o2::ccdb::BasicCCDBManager::instance();
  ccdbmgr.setCaching(true);
  ccdbmgr.setFatalWhenNull(false);
  ccdbmgr.setURL("http://alice-ccdb.cern.ch");
  auto runDuration = ccdbmgr.getRunDuration(run);
  auto tRun = runDuration.first + (runDuration.second - runDuration.first) / 2; // time stamp for the middle of the run duration
  const auto [sor, eor] = ccdbmgr.getRunDuration(run);
  const long timeMeanRun = (sor + eor) / 2.; //The same timestamp value that in getscalersforrun
  /*
  std::cout << "Timestamp:" << timeMeanRun << std::endl;
  std::cout << "sor:" << sor << std::endl;
  std::cout << "eor:" << eor << std::endl;
  */
  ccdbmgr.setTimestamp(tRun);

  // CTP orbit reset time, does not change during the run
  const auto orbitResetTimeNS = ccdbmgr.get<std::vector<int64_t>>("CTP/Calib/OrbitReset");
  const int64_t orbitResetTimeMS = (*orbitResetTimeNS)[0] * 1e-3;
  LOGP(info, "Orbit reset time in MS is {}", orbitResetTimeMS);
  
  auto scl = ccdbmgr.getSpecific<o2::ctp::CTPRunScalers>(fmt::format("CTP/Calib/Scalers/runNumber={}", run), timeMeanRun);
  // auto grp = ccdbmgr.getSpecific<o2::parameters::GRPECSObject>(fmt::format("GLO/Config/GRPECS", run), timeMeanRun);
  auto grplhc = ccdbmgr.getSpecific<o2::parameters::GRPLHCIFData>("GLO/Config/GRPLHCIF", timeMeanRun);
  const auto collidingBunches = grplhc->getBunchFilling().getNBunches();

  // automatically decice on class and factor
  float effTrigger = 0.759;
  if (classindex < 0 || type < 0 || factor < 0) {
    const auto beamA = grplhc->getBeamZ(o2::constants::lhc::BeamA);
    const auto beamC = grplhc->getBeamZ(o2::constants::lhc::BeamC);
    const auto eCM = grplhc->getSqrtS();

    std::string beamType = "p-X";

    if (beamA == 82 && beamC == 82) {
      classindex = 25;
      type = 7;
      beamType = "Pb-Pb";
      effTrigger = 28.;
    } else {
      classindex = 0;
      type = 1;
      if (eCM < 1000) {
        effTrigger = 0.68;
      } else if (eCM < 6000) {
        effTrigger = 0.737;
      } else {
        effTrigger = 0.759;
      }
    }
    LOGP(info, "Using setting for run {}: beam type = {},  class index = {}, class type = {}, effTrigger = {}", run, beamType, classindex, type, effTrigger);
  }

  //auto h2 = new TH1F("h2", "", 100, 0, 0);
  //auto gr = new TGraph;
  scl->convertRawToO2();
  const auto& ro2 = scl->getScalerRecordO2();
  int inp = 26;
  const auto firstTFOrbit = ro2.front().intRecord.orbit;
  std::vector<double> rates;  // Vector to store all rates
  std::accumulate(std::next(ro2.begin()), ro2.end(), ro2.at(0),
                  [&](const auto& first, const auto& second) {
                    const auto orbitToSec = o2::constants::lhc::LHCBunchSpacingMUS * o2::constants::lhc::LHCMaxBunches * 1e-6;
                    const auto dTime = (second.intRecord.orbit - first.intRecord.orbit) * orbitToSec;
                    double timeMS = (second.intRecord.orbit + first.intRecord.orbit) / 2 * orbitToSec;
                    if (timeType == 0) {
                      timeMS += orbitResetTimeMS / 1000.;
                    } else if (timeType == 1) {
                      timeMS -= firstTFOrbit * orbitToSec;
                    } else if (timeType == 2) {
                      timeMS -= firstTFOrbit * orbitToSec;
                      timeMS /= 60.;
                    }

                    auto rate = getScalerRate(first, second, classindex, type);
                    if (classindex < 0 || type < 0 || factor < 0) {
                      const auto mu = -std::log(1. - rate / o2::constants::lhc::LHCRevFreq / collidingBunches) / effTrigger;
                      rate = collidingBunches * mu * o2::constants::lhc::LHCRevFreq;
                    } else {
                      rate *= factor;
                    }
                    //h2->Fill(rate);
                    //gr->AddPoint(timeMS, rate);
                    rates.push_back(rate);
                    return std::move(second);
                  });

  // Average of all rates excluding the first 3 and last 3
  double meanExcludingEnds = 0;
  for (size_t i = 3; i < rates.size() - 3; ++i) {
      meanExcludingEnds += rates[i];
  }
  meanExcludingEnds /= (rates.size() - 6);

  // Average of the rates from the 4th to the 9th (start)
  double mean4To9 = 0;
  for (size_t i = 3; i <= 8; ++i) {
      mean4To9 += rates[i];
  }
  mean4To9 /= 6;

  // Average of the 6 rates of the middle of the run
  double meanMiddle6 = 0;
  size_t middleStart;
  size_t middleEnd;
  if (rates.size() % 2 == 1) {
      middleStart = (rates.size() / 2) -2.5;
      middleEnd = (rates.size() / 2) + 2.5;
  } else {
      middleStart = (rates.size() / 2) - 3;
      middleEnd = (rates.size() / 2) + 3;
  }
  for (size_t i = middleStart; i < middleEnd; ++i) {
      meanMiddle6 += rates[i];
  }
  meanMiddle6 /= 6;

  // Average of the last 6 rates excluding the last 3 (end)
  double meanLast6ExcludingLast3 = 0;
  for (size_t i = rates.size() - 9; i < rates.size() - 3; ++i) {
      meanLast6ExcludingLast3 += rates[i];
  }
  meanLast6ExcludingLast3 /= 6;

  std::cout << "IR_avg: " << meanExcludingEnds << " Hz" << std::endl;
  std::cout << "IR_start: " << mean4To9 << " Hz" << std::endl;
  std::cout << "IR_mid: " << meanMiddle6 << " Hz" << std::endl;
  std::cout << "IR_end: " << meanLast6ExcludingLast3 << " Hz" << std::endl;
  FILE *IRTxt = fopen("IR_avg_start_mid_end.txt", "w");
  fprintf(IRTxt, "%.2f\n%.2f\n%.2f\n%.2f\n", meanExcludingEnds, mean4To9, meanMiddle6, meanLast6ExcludingLast3);
  /*
  gr->Sort();
  gr->SetMarkerStyle(20);
  gr->SetMarkerSize(0.3);
  auto ax = gr->GetXaxis();
  if (timeType == 0) {
    ax->SetTimeDisplay(1);
    ax->SetTimeFormat("#splitline{%d.%m.%y}{%H:%M:%S}");
    ax->SetLabelOffset(0.025);
    ax->SetLabelSize(0.05);
    ax->SetTitle("");
  } else if (timeType == 1) {
    ax->SetLabelSize(0.05);
    ax->SetTitle("time (sec)");
  } else if (timeType == 2) {
    ax->SetLabelSize(0.05);
    ax->SetTitle("time (min)");
  }
  return gr;
  */
}

double getScalerRate(const o2::ctp::CTPScalerRecordO2& first, const o2::ctp::CTPScalerRecordO2& second, int classindex, int type)
{
  const auto timedelta = (second.intRecord.orbit - first.intRecord.orbit) * o2::constants::lhc::LHCBunchSpacingMUS * o2::constants::lhc::LHCMaxBunches * 1e-6;
  if (type < 7) {
    auto s0 = &(first.scalers[classindex]); // type CTPScalerO2*
    auto s1 = &(second.scalers[classindex]);
    switch (type) {
      case 1:
        return (s1->lmBefore - s0->lmBefore) / timedelta;
      case 2:
        return (s1->lmAfter - s0->lmAfter) / timedelta;
      case 3:
        return (s1->l0Before - s0->l0Before) / timedelta;
      case 4:
        return (s1->l0After - s0->l0After) / timedelta;
      case 5:
        return (s1->l1Before - s0->l1Before) / timedelta;
      case 6:
        return (s1->l1After - s0->l1After) / timedelta;
      default:
        LOG(error) << "Wrong type:" << type;
        return -1; // wrong type
    }
  } else if (type == 7) {
    // LOG(info) << "doing input:";
    auto s0 = first.scalersInps[classindex]; // type CTPScalerO2*
    auto s1 = second.scalersInps[classindex];
    return (s1 - s0) / timedelta;
  } else {
    LOG(error) << "Wrong type:" << type;
    return -1; // wrong type
  }
  return -1;
}

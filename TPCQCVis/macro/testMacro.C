/*
   .L $NOTES/JIRA/ATO-611/tpcQCIDC.C


 */
#include <cmath>
#include <fmt/format.h>
#include <string_view>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <map>
#include <TMath.h>
#include <TGraph.h>
#include <TTree.h>
#include <ROOT/RVec.hxx>
#include <ROOT/RDataFrame.hxx>

#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options.hpp>

#include "Framework/Logger.h"
#include "CCDB/CcdbApi.h"
#include "CCDB/BasicCCDBManager.h"

#include "TPCBase/CDBInterface.h"
#include "TPCBase/Utils.h"
#include "TPCBase/Mapper.h"

#include "CommonConstants/LHCConstants.h"
#include "CommonUtils/TreeStreamRedirector.h"

#include "TPCCalibration/SACDecoder.h"
#include "TPCCalibration/CalibTreeDump.h"
#include "TPCCalibration/IDCContainer.h"
#include "TPCCalibration/IDCGroupHelperSector.h"
#include "TPCCalibration/IDCCCDBHelper.h"
#include "TPCCalibration/SACCCDBHelper.h"
#include "TPCCalibration/IDCFactorization.h"
#include "TPCCalibration/RobustAverage.h"
#include "DataFormatsTPC/VDriftCorrFact.h"
#include "GRPCalibration/GRPDCSDPsProcessor.h"

#include "QualityControl/TPC/ClustersData.h"
#include "QualityControl/TPC/DCSPTemperature.h"

#define BOOST_BIND_GLOBAL_PLACEHOLDERS 1

const int NSECTORS = 36;
const int NROCS = 72;
const int IDCMAXFILES = 9999;   // 9999;
const int IDCDELTAMAXFILES = 1; // restrict IDCDelta currently to one file only, otherwise it will take very long
const int NSENSORS = 18;

#ifdef __MAKECINT__
#pragma link C++ class ROOT::VecOps::RVec < ROOT::VecOps::RVec < float>> + ;
#pragma link C++ class ROOT::VecOps::RVec < ROOT::VecOps::RVec < double>> + ;
#pragma link C++ class ROOT::VecOps::RVec < ROOT::VecOps::RVec < long>> + ;
#pragma link C++ class ROOT::VecOps::RVec < o2::tpc::dcs::Gas> + ;
#pragma link C++ class ROOT::VecOps::RVec < o2::tpc::VDriftCorrFact> + ;
#endif

using namespace o2::tpc;
using boost::property_tree::ptree;
using QCCL = o2::quality_control_modules::tpc::ClustersData;
using dataT = unsigned char;

// Function to extract configuration from config file
std::map<std::string, std::vector<std::string>> getConfiguration(const std::string configFile);
std::vector<std::map<std::string, std::vector<std::string>>> getConfigurations(const std::string configFile);

// Function to add objects to file
void addObjects(std::map<std::string, std::vector<std::string>> config, TTree* tree, const int runTmp = -1, const bool writeDataCounter = true);

// Function to get the End-Of-Run and Start-Of-Run timestamps for given run
/// \param offsStart can be used to specify an offset in respect to the start of run in seconds (0 start from the beginning of the run)
/// \param offsEnd can be used to specify an offset in respect to the start+offsStart of run in seconds (-1 select the full run)
std::vector<long> getSorEor(const int run, const long offsStart = 0, const long offsEnd = -1);

// Function to get vector of all valid instances (timestamps) of object in CDB within given range
std::vector<long> getTimestamps(o2::ccdb::CcdbApi mCDB, const std::string path, const std::vector<long> period);

// Function to apply a per pad normalization to CalDet
void normalizeCalDets(std::vector<CalDet<float>>& mCalDetObjects);
void normalizeCalDet(CalDet<float>& calDet);

// Functions to get per-pad Mean, Median or RMS CalDet object from the given vector of CalDet objects
void getMeanCalDets(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects);
void getMedianCalDets(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects);
void getRMSCalDets(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects);

// Get mean, median or RMS of calDet object
float getMeanCalDet(CalDet<float>& calDet);
float getMedianCalDet(CalDet<float>& calDet);
float getRMSCalDet(CalDet<float>& calDet);

// Functions to get time series from vector of CalDet objects
std::vector<float> getTimeSeriesMean(const std::vector<CalDet<float>>& mCalDetObjects, float min = 0, float max = 999999999);
std::vector<float> getTimeSeriesMedian(const std::vector<CalDet<float>>& mCalDetObjects);
std::vector<float> getTimeSeriesRMS(const std::vector<CalDet<float>>& mCalDetObjects);

std::array<std::vector<float>, NSECTORS> getTimeSeriesMeanSector(const std::vector<CalDet<float>>& mCalDetObjects);
std::array<std::vector<float>, NSECTORS> getTimeSeriesMedianSector(const std::vector<CalDet<float>>& mCalDetObjects);
std::array<std::vector<float>, NSECTORS> getTimeSeriesRMSSector(const std::vector<CalDet<float>>& mCalDetObjects);

std::array<std::vector<float>, NROCS> getTimeSeriesMeanROC(const std::vector<CalDet<float>>& mCalDetObjects);
std::array<std::vector<float>, NROCS> getTimeSeriesMedianROC(const std::vector<CalDet<float>>& mCalDetObjects);
std::array<std::vector<float>, NROCS> getTimeSeriesRMSROC(const std::vector<CalDet<float>>& mCalDetObjects);

std::vector<float> calDetToVec(const CalDet<float>& calDet);

void addMappingInfo(TTree* tree);

void setDefaultAliases(TTree* tree);

o2::ccdb::CcdbApi mCDB;

// Main
void tpcQCIDC(const std::string configFile, const int run = -1)
{
  std::string filename = fmt::format("out_{}.root", run);
  TTree* tree = new TTree("tree", "Tree containing QC observables");
  // Read config
  auto configs = getConfigurations(configFile);

  std::cout << "Adding mapping info for pad-wise data" << std::endl;
  addMappingInfo(tree);

  bool writeDataCounter = true;
  for (auto& config : configs) {
    std::cout << "Using config file: " << configFile << ". Read options: " << std::endl;
    for (const auto& myPair : config) {
      std::cout << myPair.first << " : " << myPair.second.at(0) << std::endl;
    }
    // Get and add all objects according to config
    addObjects(config, tree, run, writeDataCounter);

    // write meta data only once
    writeDataCounter = false;
  }

  setDefaultAliases(tree);
  TFile* file(TFile::Open(filename.c_str(), "RECREATE"));
  tree->Write();
  std::cout << "Closing file" << std::endl;
  file->Close();
  return;
}

void addObjects(std::map<std::string, std::vector<std::string>> config, TTree* tree, const int runTmp, const bool writeDataCounter)
{

  std::vector<std::pair<std::vector<CalDet<float>>, std::vector<long>>> calDetMatrix;

  // Retrieve all CalDet objects
  for (size_t iObject = 0; iObject < config["object"].size(); iObject++) {
    std::string object = config["object"].at(iObject);
    std::cout << "Will get objects for " << object << std::endl;

    if (mCDB.getURL() != config["input"].at(iObject)) {
      std::cout << "Accessing new DB @ " << config["input"].at(iObject) << std::endl;
      mCDB.init(config["input"].at(iObject));
    }
    std::map<std::string, std::string> metadata;

    std::vector<long> period;
    for (size_t iRun = 0; iRun < config["runNumber"].size(); iRun++) {
      int run = (runTmp < 0) ? std::stoi(config["runNumber"].at(iRun)) : runTmp;

      // writing meta data only once
      if (iObject == 0 && writeDataCounter) {
        TBranch* bRun = tree->Branch("run", &run);
        bRun->Fill();
      }

      std::vector<std::string> confTimeRange = config["timeRange"];
      assert(confTimeRange.size() != 2);
      const int offsStart = std::stoi(confTimeRange[0]);
      const int offsEnd = std::stoi(confTimeRange[1]);
      std::vector<long> sorEor = getSorEor(run, offsStart, offsEnd);
      period.insert(period.end(), sorEor.begin(), sorEor.end());
      std::cout << "Looking at run: " << run << " for object: " << object << std::endl;

      std::string path;

      if (object == "Clusters") {
        path = "qc/TPC/MO/Clusters/ClusterData";
        std::vector<long> times = getTimestamps(mCDB, path, period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for Clusters." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        long timestamp;
        std::vector<CalDet<float>> clusterNClusters(nTimes);
        std::vector<CalDet<float>> clusterQMax(nTimes);
        std::vector<CalDet<float>> clusterQTot(nTimes);
        std::vector<CalDet<float>> clusterSigmaTime(nTimes);
        std::vector<CalDet<float>> clusterSigmaPad(nTimes);
        std::vector<CalDet<float>> clusterTimeBin(nTimes);

        for (int i = 0; i < nTimes; i++) {
          long timestamp = times[i];
          auto qccl = mCDB.retrieveFromTFileAny<QCCL>(path, metadata, timestamp);
          auto& clInfo = qccl->getClusters();
          clusterNClusters[i] = clInfo.getNClusters();
          clusterNClusters[i].setName("NClusters_Clusters");
          clusterQMax[i] = clInfo.getQMax();
          clusterQMax[i].setName("QMax_Clusters");
          clusterQTot[i] = clInfo.getQTot();
          clusterQTot[i].setName("QTot_Clusters");
          clusterSigmaTime[i] = clInfo.getSigmaTime();
          clusterSigmaTime[i].setName("SigmaTime_Clusters");
          clusterSigmaPad[i] = clInfo.getSigmaPad();
          clusterSigmaPad[i].setName("SigmaPad_Clusters");
          clusterTimeBin[i] = clInfo.getTimeBin();
          clusterTimeBin[i].setName("TimeBin_Clusters");
        }

        calDetMatrix.push_back(std::make_pair(clusterNClusters, times));
        calDetMatrix.push_back(std::make_pair(clusterQMax, times));
        calDetMatrix.push_back(std::make_pair(clusterQTot, times));
        calDetMatrix.push_back(std::make_pair(clusterSigmaTime, times));
        calDetMatrix.push_back(std::make_pair(clusterSigmaPad, times));
        calDetMatrix.push_back(std::make_pair(clusterTimeBin, times));

        std::cout << "Adding " << nTimes << " calDets from ClusterData to be processed." << std::endl;
      } else if (object == "RawDigits") {
        path = "qc/TPC/MO/RawDigits/RawDigitData";
        std::vector<long> times = getTimestamps(mCDB, path, period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for RawDigits." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        // long timestamp;
        std::vector<CalDet<float>> digitNClusters(nTimes);
        std::vector<CalDet<float>> digitQMax(nTimes);

        for (int i = 0; i < nTimes; i++) {
          long timestamp = times[i];
          auto qccl = mCDB.retrieveFromTFileAny<QCCL>(path, metadata, timestamp);
          auto& clInfo = qccl->getClusters();
          digitNClusters[i] = clInfo.getNClusters();
          digitNClusters[i].setName("NClusters_Digits");
          digitQMax[i] = clInfo.getQMax();
          digitQMax[i].setName("QMax_Digits");
        }

        calDetMatrix.push_back(std::make_pair(digitNClusters, times));
        calDetMatrix.push_back(std::make_pair(digitQMax, times));

        std::cout << "Adding " << nTimes << " calDets from RawDigitData to be processed." << std::endl;
      } else if (object == "IDCZero") {
        path = CDBTypeMap.at(CDBType::CalIDC0A); // "TPC/Calib/IDC_0_A";
        std::vector<long> times = getTimestamps(mCDB, path, period);
        if (times.size() > IDCMAXFILES) {
          times.resize(IDCMAXFILES);
        }

        int nTimes = (int)times.size();
        std::cout << "Found " << nTimes << " objects matching the config for IDC_0." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        // long timestamp;
        IDCCCDBHelper<dataT> helper;
        std::vector<CalDet<float>> idcZeros(nTimes);
        std::vector<CalDet<float>> idcZerosPadFlag(nTimes);

        for (int i = 0; i < nTimes; i++) {
          idcZerosPadFlag[i] = CalPad("IDCPadFlags", PadSubset::ROC);

          long timestamp = times[i];
          auto mIDCZeroA = mCDB.retrieveFromTFileAny<o2::tpc::IDCZero>(path, metadata, timestamp);
          auto mIDCZeroC = mCDB.retrieveFromTFileAny<o2::tpc::IDCZero>(CDBTypeMap.at(CDBType::CalIDC0C), metadata, timestamp);

          helper.setIDCZero(mIDCZeroA, Side::A);
          helper.setIDCZero(mIDCZeroC, Side::C);
          helper.createOutlierMap(); // create outlier map for the IDC0 which are currently set

          for (unsigned int sector = 0; sector < Mapper::NSECTORS; ++sector) {
            for (unsigned int region = 0; region < Mapper::NREGIONS; ++region) {
              for (unsigned int irow = 0; irow < Mapper::ROWSPERREGION[region]; ++irow) {
                for (unsigned int ipad = 0; ipad < Mapper::PADSPERROW[region][irow]; ++ipad) {
                  // auto flag = static_cast<float>(helper.getPadStatusMap()->getCalArray(region + sector * Mapper::NREGIONS).getValue(Mapper::OFFSETCRULOCAL[region][irow] + ipad));
                  // idcZerosPadFlag[i].setValue(sector, Mapper::ROWOFFSET[region] + irow, ipad, flag);
                }
              }
            }
          }

          // const bool rejectOutlier = true;
          // float scalingValIDCA = helper.scaleIDC0(Side::A, rejectOutlier);
          // float scalingValIDCC = helper.scaleIDC0(Side::C, rejectOutlier);

          idcZeros[i] = helper.getIDCZeroCalDet();
        }

        // store outlier map only in case the IDCs for each pad are stored in the output TTree
        std::vector<std::string> confPerPad = config["exportPerPad"];
        if (std::find(confPerPad.begin(), confPerPad.end(), "All") != confPerPad.end()) {
          std::string nameBr = "Outlier_IDC0";
          std::vector<float> outVec;
          ROOT::RVec<float> outRVec(outVec.data(), outVec.size());
          TBranch* bAll = tree->Branch(nameBr.c_str(), &outRVec);
          for (auto& calDet : idcZerosPadFlag) {
            outVec = calDetToVec(calDet);
            outRVec = outVec;
            bAll->Fill();
          }
        }

        calDetMatrix.push_back(std::make_pair(idcZeros, times));
      }
      // Need to find a way to convert SAC (value per stack) to CalPad (value per pad)
      else if (object == "SACZero") {
        path = CDBTypeMap.at(CDBType::CalSAC0); //"TPC/Calib/SAC_0";
        std::vector<long> times = getTimestamps(mCDB, path, period);
        if (times.size() > IDCMAXFILES) {
          times.resize(IDCMAXFILES);
        }

        int nTimes = (int)times.size();
        std::cout << "Found " << nTimes << " objects matching the config for SAC_0." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        // long timestamp;
        SACCCDBHelper<dataT> helperSAC;
        CalDet<float> sacZeroCalDet("SAC0");
        std::vector<CalDet<float>> sacZeros(nTimes);

        for (int i = 0; i < nTimes; i++) {
          long timestamp = times[i];
          auto mSACZero = mCDB.retrieveFromTFileAny<o2::tpc::SACZero>(path, metadata, timestamp);
          helperSAC.setSACZero(mSACZero);

          for (unsigned int cru = 0; cru < CRU::MaxCRU; ++cru) {
            const o2::tpc::CRU cruTmp(cru);
            const Side side = cruTmp.side();
            const int region = cruTmp.region();
            const int sector = cruTmp.sector();
            const auto sacZero = helperSAC.getSACZeroVal(sector, cruTmp.gemStack());
            for (unsigned int lrow = 0; lrow < Mapper::ROWSPERREGION[region]; ++lrow) {
              for (unsigned int pad = 0; pad < Mapper::PADSPERROW[region][lrow]; ++pad) {
                sacZeroCalDet.setValue(sector, Mapper::ROWOFFSET[region] + lrow, pad, sacZero);
              }
            }
          }
          sacZeros[i] = sacZeroCalDet;
        }
        calDetMatrix.push_back(std::make_pair(sacZeros, times));
      } else if (object == "IDCOne") {
        for (int side = 0; side < 2; ++side) {
          path = (side == 0) ? CDBTypeMap.at(CDBType::CalIDC1A) : CDBTypeMap.at(CDBType::CalIDC1C);
          std::vector<long> times = getTimestamps(mCDB, path, period);
          if (times.size() > IDCMAXFILES) {
            times.resize(IDCMAXFILES);
          }

          int nTimes = (int)times.size();
          std::cout << "Found " << nTimes << " objects matching the config for IDC_1." << std::endl;

          if (nTimes == 0) {
            continue;
          }

          // for the IDC1 (define on top and fill after the loop if more than one entry)
          ROOT::RVec<float> idcOne;
          ROOT::RVec<double> timeStampsIDC1;
          for (int i = 0; i < nTimes; i++) {
            long timestamp = times[i];
            const std::vector<float> idcOneTmp = mCDB.retrieveFromTFileAny<o2::tpc::IDCOne>(path, metadata, timestamp)->mIDCOne;
            idcOne.insert(idcOne.end(), idcOneTmp.begin(), idcOneTmp.end());

            // assume same timestamps for IDC A and C side
            for (size_t ii = 0; ii < idcOneTmp.size(); ++ii) {
              timeStampsIDC1.emplace_back((timestamp + ii * 12 /*12 orbits integration interval per IDC*/ * o2::constants::lhc::LHCOrbitMUS * 0.001) / 1000.);
            }
          }

          // Fill directly in the TTree
          std::string sSide = (side == 0) ? "A" : "C";
          TBranch* bIDCOne = tree->Branch(fmt::format("IDC1_{}", sSide).data(), &idcOne);
          TBranch* bIDCOneTime = tree->Branch(fmt::format("IDC1_{}_time", sSide).data(), &timeStampsIDC1);
          bIDCOne->Fill();
          bIDCOneTime->Fill();
        }
      } else if (object == "SACOne") {
        path = CDBTypeMap.at(CDBType::CalSAC1);
        std::vector<long> times = getTimestamps(mCDB, path, period);
        if (times.size() > IDCMAXFILES) {
          times.resize(IDCMAXFILES);
        }

        int nTimes = (int)times.size();
        std::cout << "Found " << nTimes << " objects matching the config for SAC_1." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        // for the IDC1 (define on top and fill after the loop if more than one entry)
        ROOT::RVec<float> sacOneA;
        ROOT::RVec<float> sacOneC;
        ROOT::RVec<double> timeStampsSAC1;

        for (int i = 0; i < nTimes; i++) {
          long timestamp = times[i];
          auto sacOneTmp = mCDB.retrieveFromTFileAny<o2::tpc::SACOne>(path, metadata, timestamp);
          const auto& sacOneATmp = sacOneTmp->mSACOne[Side::A].mIDCOne;
          const auto& sacOneCTmp = sacOneTmp->mSACOne[Side::C].mIDCOne;
          sacOneA.insert(sacOneA.end(), sacOneATmp.begin(), sacOneATmp.end());
          sacOneC.insert(sacOneC.end(), sacOneCTmp.begin(), sacOneCTmp.end());

          // assume same timestamps for IDC A and C side
          for (size_t ii = 0; ii < sacOneATmp.size(); ++ii) {
            timeStampsSAC1.emplace_back((timestamp + ii * o2::tpc::sac::Decoder::SampleDistanceTimeMS /* ~1ms */) / 1000.);
          }
        }

        // Fill directly in the TTree
        TBranch* bSACOneA = tree->Branch("SAC1_A", &sacOneA);
        TBranch* bSACOneC = tree->Branch("SAC1_C", &sacOneC);
        TBranch* bSACOneTime = tree->Branch("SAC1_time", &timeStampsSAC1);
        bSACOneA->Fill();
        bSACOneC->Fill();
        bSACOneTime->Fill();
      } else if (object == "IDCDelta") {
        Side side = Side::A;

        // fetching time stamps for delta IDCs
        const std::string pathA = CDBTypeMap.at(CDBType::CalIDCDeltaA);
        const std::string pathC = CDBTypeMap.at(CDBType::CalIDCDeltaC);
        std::vector<long> timesA = getTimestamps(mCDB, pathA, period);
        std::vector<long> timesC = getTimestamps(mCDB, pathC, period);
        if (timesA.size() > IDCDELTAMAXFILES) {
          timesA.resize(IDCDELTAMAXFILES);
          timesC.resize(IDCDELTAMAXFILES);
        }

        int nTimes = (int)timesA.size();
        std::cout << "Found " << nTimes << " objects matching the config for IDC_Delta." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        std::vector<CalDet<float>> idcDeltasCalDet;
        std::vector<CalDet<float>> idcDeltas1DStack;
        std::vector<long> timeStampsDelta;
        ROOT::RVec<ROOT::RVec<float>> idcDeltaMean(GEMSTACKS);
        ROOT::RVec<ROOT::RVec<float>> idcDeltaMedian(GEMSTACKS);

        // setting up grouping parameter for IDC Delta
        IDCCCDBHelper<dataT> helper;
        std::unique_ptr<IDCGroupHelperSector> groupingHelper = std::make_unique<IDCGroupHelperSector>(IDCGroupHelperSector{*mCDB.retrieveFromTFileAny<o2::tpc::ParameterIDCGroupCCDB>(CDBTypeMap.at(CDBType::CalIDCGroupingParA), metadata, timesA.front())});
        helper.setGroupingParameter(groupingHelper.get(), Side::A);
        helper.setGroupingParameter(groupingHelper.get(), Side::C);

        // Setting up the class for factorization of the IDCs per stack
        const unsigned int timeframes = nTimes;
        std::vector<IDCFactorization> idcFac;
        std::vector<std::vector<uint32_t>> crus(GEMSTACKS);
        for (int iCRU = 0; iCRU < CRU::MaxCRU; ++iCRU) {
          o2::tpc::CRU cru(iCRU);
          const int stack = static_cast<int>(cru.gemStack()) + cru.sector() * GEMSTACKSPERSECTOR;
          crus[stack].emplace_back(iCRU);
        }

        for (int stack = 0; stack < GEMSTACKS; ++stack) {
          idcFac.emplace_back(o2::tpc::IDCFactorization(timeframes, timeframes, crus[stack]));
          idcFac.back().setUsePadStatusMap(false); // no outlier removal
        }

        // loop over IDCDelta
        for (int i = 0; i < nTimes; i++) {
          std::cout << "Processed: " << i << " out of: " << nTimes - 1 << std::endl;
          long timestampA = timesA[i];
          long timestampC = timesC[i];

          if (timestampA != timestampC) {
            std::cout << "WARN: time stamp for A and C side is different! TODO improve this!!!" << std::endl;
          }

          auto idcDeltaA = mCDB.retrieveFromTFileAny<o2::tpc::IDCDelta<dataT>>(pathA, metadata, timestampA);
          auto idcDeltaC = mCDB.retrieveFromTFileAny<o2::tpc::IDCDelta<dataT>>(pathC, metadata, timestampC);
          helper.setIDCDelta(idcDeltaA, Side::A);
          helper.setIDCDelta(idcDeltaC, Side::C);

          // convert the IDCDelta coarse granularity to pad granularity
          std::vector<CalDet<float>> idcDeltasCalDetTmp = helper.getIDCDeltaCalDet();

          // number of CalDets stored
          const auto nTimeStampsDelta = idcDeltasCalDetTmp.size();

          // move the caldets to the vector
          idcDeltasCalDet.insert(idcDeltasCalDet.end(), std::make_move_iterator(idcDeltasCalDetTmp.begin()), std::make_move_iterator(idcDeltasCalDetTmp.end()));
          idcDeltasCalDetTmp.erase(idcDeltasCalDetTmp.begin(), idcDeltasCalDetTmp.end());

          // store the time stamp for each caldet
          for (size_t ii = 0; ii < nTimeStampsDelta; ++ii) {
            timeStampsDelta.emplace_back(timestampA + ii * 12 /*12 orbits integration interval per IDC*/ * o2::constants::lhc::LHCOrbitMUS * 0.001);
          }

          // perform the factorization: calculate IDCs back -> fill IDCFactorize per stack -> perform factorization
          bool doFactorization = true;
          if (doFactorization) {
            auto idcOneATmp = mCDB.retrieveFromTFileAny<IDCOne>(CDBTypeMap.at(CDBType::CalIDC1A), metadata, timestampA);
            auto idcOneCTmp = mCDB.retrieveFromTFileAny<IDCOne>(CDBTypeMap.at(CDBType::CalIDC1C), metadata, timestampA);
            auto idcZeroATmp = mCDB.retrieveFromTFileAny<IDCZero>(CDBTypeMap.at(CDBType::CalIDC0A), metadata, timestampA);
            auto idcZeroCTmp = mCDB.retrieveFromTFileAny<IDCZero>(CDBTypeMap.at(CDBType::CalIDC0C), metadata, timestampA);
            helper.setIDCOne(idcOneATmp, Side::A);
            helper.setIDCOne(idcOneCTmp, Side::C);
            helper.setIDCZero(idcZeroATmp, Side::A);
            helper.setIDCZero(idcZeroCTmp, Side::C);
            // helper.dumpToTree();
            // helper.dumpToTreeIDCDelta();

            std::vector<long> ts(4);
            ts[0] = std::stol(mCDB.retrieveHeaders(CDBTypeMap.at(CDBType::CalIDC1A), metadata, timestampA)["Valid-From"]);
            ts[1] = std::stol(mCDB.retrieveHeaders(CDBTypeMap.at(CDBType::CalIDC1C), metadata, timestampA)["Valid-From"]);
            ts[2] = std::stol(mCDB.retrieveHeaders(CDBTypeMap.at(CDBType::CalIDC0A), metadata, timestampA)["Valid-From"]);
            ts[3] = std::stol(mCDB.retrieveHeaders(CDBTypeMap.at(CDBType::CalIDC0C), metadata, timestampA)["Valid-From"]);
            // compare the timestamp, which should be all the same
            if (!std::equal(ts.begin() + 1, ts.end(), ts.begin())) {
              LOGP(warning, "Processing CalIDC1A {} CalIDC1C {}  CalIDC0A {}  CalIDC0C {}", ts[0], ts[1], ts[2], ts[3]);
            } else {
              LOGP(info, "Time difference between IDCDelta and 1D-IDCs {}", timestampA - ts.front());
            }

            for (int iCRU = 0; iCRU < CRU::MaxCRU; ++iCRU) {
              const o2::tpc::CRU cru(iCRU);
              const int region = cru.region();
              const int stack = static_cast<int>(cru.gemStack()) + cru.sector() * GEMSTACKSPERSECTOR;
              std::vector<float> idcCRU;
              idcCRU.reserve(Mapper::PADSPERREGION[region] * nTimeStampsDelta);

              for (unsigned int integrationInterval = 0; integrationInterval < nTimeStampsDelta; ++integrationInterval) {
                o2::tpc::RobustAverage average(Mapper::PADSPERREGION[region]);
                for (unsigned int irow = 0; irow < Mapper::ROWSPERREGION[region]; ++irow) {
                  for (unsigned int ipad = 0; ipad < Mapper::PADSPERROW[region][irow]; ++ipad) {
                    const float idc = helper.getIDCVal(cru.sector(), cru.region(), irow, ipad, integrationInterval);
                    // const float idc = helper.getIDCDeltaVal(cru.sector(), cru.region(), irow, ipad, integrationInterval);
                    idcCRU.emplace_back(idc);
                    average.addValue(idc);
                  }
                }
              }
              idcFac[stack].setIDCs(std::move(idcCRU), iCRU, i);
            }
          }

          o2::tpc::RobustAverage averageA;
          o2::tpc::RobustAverage averageC;
          const int stacksPerSide = GEMSTACKS / 2;
          for (int stack = 0; stack < stacksPerSide; ++stack) {

            const unsigned int sector = stack / GEMSTACKSPERSECTOR;
            const unsigned int stackInSec = stack % GEMSTACKSPERSECTOR;
            int startRegion = 0;
            int endRegion = 4;

            if (stackInSec == 1) {
              startRegion = 4;
            } else if (stackInSec == 1) {
              startRegion = 6;
            } else if (stackInSec == 1) {
              startRegion = 8;
            }

            if (stackInSec != 0) {
              endRegion = startRegion + 2;
            }

            for (unsigned int integrationInterval = 0; integrationInterval < nTimeStampsDelta; ++integrationInterval) {
              for (int region = startRegion; region < endRegion; ++region) {
                const long indexStart = SECTORSPERSIDE * integrationInterval * groupingHelper->getNIDCsPerSector() + sector * groupingHelper->getNIDCsPerSector() + groupingHelper->getRegionOffset(region);
                const long indexEnd = indexStart + groupingHelper->getNIDCs(region);

                for (long j = indexStart; j < indexEnd; ++j) {
                  averageA.addValue(idcDeltaA->getValue(j));
                  averageC.addValue(idcDeltaC->getValue(j));
                }
              }
              idcDeltaMean[stack].emplace_back(averageA.getMean());
              idcDeltaMean[stack + stacksPerSide].emplace_back(averageC.getMean());

              idcDeltaMedian[stack].emplace_back(averageA.getMedian());
              idcDeltaMedian[stack + stacksPerSide].emplace_back(averageC.getMedian());

              averageA.clear();
              averageC.clear();
            }
          }
        }

        // for (int stack = 0; stack < 4 /*GEMSTACKS*/; ++stack) {
        // idcFac[stack].dumpLargeObjectToFile(fmt::format("IDC_stack{}.root", stack).data());
        // }
        // dump the 1Ds to disk

        ROOT::RVec<ROOT::RVec<float>> idcOne;
        ROOT::RVec<ROOT::RVec<long>> timeStampsIDC1Stack;
        // ignore info messages
        fair::Logger::SetConsoleSeverity(fair::Severity::warning);
        for (int stack = 0; stack < GEMSTACKS; ++stack) {
          idcFac[stack].factorizeIDCs(false, false);
          idcOne.emplace_back(idcFac[stack].getIDCOneVec(idcFac[stack].getSides().front()));
          timeStampsIDC1Stack.emplace_back(timeStampsDelta);
        }
        // enable info messages again
        fair::Logger::SetConsoleSeverity(fair::Severity::info);

        TBranch* b1DIDCStack = tree->Branch("IDC1_Stack", &idcOne);
        TBranch* b1DIDCStackTime = tree->Branch("IDC1_Stack_time", &timeStampsIDC1Stack);
        TBranch* bIDCDeltaMean = tree->Branch("IDCDelta_Stack_Mean", &idcDeltaMean);
        TBranch* bIDCDeltaMedian = tree->Branch("IDCDelta_Stack_Median", &idcDeltaMedian);
        b1DIDCStack->Fill();
        b1DIDCStackTime->Fill();
        bIDCDeltaMean->Fill();
        bIDCDeltaMedian->Fill();
        calDetMatrix.push_back(std::make_pair(idcDeltasCalDet, timeStampsDelta));
      } else if (object == "SACDelta") {
        path = CDBTypeMap.at(CDBType::CalSACDelta);
        std::vector<long> times = getTimestamps(mCDB, path, period);
        if (times.size() > IDCDELTAMAXFILES) {
          times.resize(IDCDELTAMAXFILES);
        }

        int nTimes = (int)times.size();
        std::cout << "Found " << nTimes << " objects matching the config for SAC_Delta." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        ROOT::RVec<ROOT::RVec<float>> sacs(GEMSTACKS);
        ROOT::RVec<ROOT::RVec<float>> sacDelta(GEMSTACKS);
        ROOT::RVec<ROOT::RVec<long>> timeStampsDelta(GEMSTACKS);

        SACCCDBHelper<dataT> helper;
        for (int i = 0; i < nTimes; i++) {
          std::cout << "Processed: " << i << " out of: " << nTimes << std::endl;
          long timestamp = times[i];
          auto sacDeltaTmp = mCDB.retrieveFromTFileAny<o2::tpc::SACDelta<dataT>>(path, metadata, timestamp);
          auto idc1D = mCDB.retrieveFromTFileAny<o2::tpc::SACOne>(CDBTypeMap.at(CDBType::CalSAC1), metadata, timestamp);
          auto idc0D = mCDB.retrieveFromTFileAny<o2::tpc::SACZero>(CDBTypeMap.at(CDBType::CalSAC0), metadata, timestamp);
          helper.setSACDelta(sacDeltaTmp);
          helper.setSACOne(idc1D);
          helper.setSACZero(idc0D);
          // helper.dumpToTree();

          const int nintervals = helper.getNIntegrationIntervalsSACDelta(Side::A);
          for (int interval = 0; interval < nintervals; ++interval) {
            for (int stack = 0; stack < GEMSTACKS; ++stack) {
              const unsigned int sector = stack / GEMSTACKSPERSECTOR;
              const unsigned int stackInSec = stack % GEMSTACKSPERSECTOR;
              sacs[stack].emplace_back(helper.getSACVal(sector, stackInSec, interval));
              sacDelta[stack].emplace_back(helper.getSACDeltaVal(sector, stackInSec, interval));
              timeStampsDelta[stack].emplace_back(timestamp + interval * o2::tpc::sac::Decoder::SampleDistanceTimeMS);
            }
          }
        }

        TBranch* bSAC = tree->Branch("SAC_Stack", &sacs);
        TBranch* bDeltaSAC = tree->Branch("SAC_Delta_Stack", &sacDelta);
        TBranch* bTimeSAC = tree->Branch("SAC_Stack_time", &timeStampsDelta);
        bSAC->Fill();
        bDeltaSAC->Fill();
        bTimeSAC->Fill();
      } else if (object == "Temperature") {
        path = CDBTypeMap.at(CDBType::CalTemperature);
        std::vector<long> times = getTimestamps(mCDB, path, period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for RawDigits." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        ROOT::RVec<ROOT::RVec<float>> temperatureRVec(NSENSORS);
        ROOT::RVec<ROOT::RVec<long>> timestampsRVec(NSENSORS);

        for (int i = 0; i < nTimes; i++) {
          long timestamp = times[i];
          auto temperature = mCDB.retrieveFromTFileAny<o2::tpc::dcs::Temperature>(path, metadata, timestamp);
          int sensorCounter = 0;
          for (const auto sensor : temperature->raw) {
            std::vector<float> temperatureVecTmp;
            std::vector<long> timestampsVecTmp;

            for (const auto& value : sensor.data) {
              temperatureVecTmp.emplace_back(value.value);
              timestampsVecTmp.emplace_back(value.time);
            }

            temperatureRVec.at(sensorCounter).append(temperatureVecTmp.begin(), temperatureVecTmp.end());
            timestampsRVec.at(sensorCounter).append(timestampsVecTmp.begin(), timestampsVecTmp.end());
            sensorCounter++;
          }
        }
        // Fill directly in the TTree
        TBranch* bTemperatureValues = tree->Branch("Temperature", &temperatureRVec);
        TBranch* bTemperatureTimes = tree->Branch("Temperature_time", &timestampsRVec);
        bTemperatureValues->Fill();
        bTemperatureTimes->Fill();

        // Add temperature metadata
        ROOT::RVec<float> tempSensorPosX = {211.40f, 82.70f, -102.40f, -228.03f, -246.96f, -150.34f, -16.63f, 175.82f, 252.74f, 228.03f, 102.40f, -71.15f, -211.40f, -252.74f, -175.82f, -16.63f, 150.34f, 252.74f};
        ROOT::RVec<float> tempSensorPosY = {141.25f, 227.22f, 232.72f, 112.45f, -60.43f, -205.04, -253.71f, -183.66f, -27.68f, 112.45f, 232.72f, 244.09f, 141.25f, -27.68f, -183.66, -253.71f, -205.04f, -27.68f};
        TBranch* bTempSensorPosX = tree->Branch("Temperature_Position_X", &tempSensorPosX);
        TBranch* bTempSensorPosY = tree->Branch("Temperature_Position_Y", &tempSensorPosY);
        bTempSensorPosX->Fill();
        bTempSensorPosY->Fill();
      } else if (object == "GRPEnvVars") {
        path = "GLO/Config/EnvVars";
        std::vector<long> times = getTimestamps(mCDB, path, period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for pressure." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        ROOT::RVec<double> cavernAtmosPressure;
        ROOT::RVec<double> cavernAtmosPressure2;
        ROOT::RVec<uint64_t> cavernAtmosPressure_time;
        ROOT::RVec<uint64_t> cavernAtmosPressure2_time;

        for (int i = 0; i < nTimes; i++) {
          std::cout << "Processed: " << i << " out of: " << nTimes << std::endl;
          long timestamp = times[i];
          auto grpEnvVarsTmp = mCDB.retrieveFromTFileAny<o2::grp::GRPEnvVariables>(path, metadata, timestamp);
          for (auto& pressurePair : grpEnvVarsTmp->mEnvVars.at("CavernAtmosPressure")) {
            cavernAtmosPressure.emplace_back(pressurePair.second);
            cavernAtmosPressure_time.emplace_back(pressurePair.first);
          }
          for (auto& pressurePair : grpEnvVarsTmp->mEnvVars.at("CavernAtmosPressure2")) {
            cavernAtmosPressure2.emplace_back(pressurePair.second);
            cavernAtmosPressure2_time.emplace_back(pressurePair.first);
          }
        }

        TBranch* bCavernAtmosPressure = tree->Branch("CavernAtmosPressure", &cavernAtmosPressure);
        bCavernAtmosPressure->Fill();
        TBranch* bCavernAtmosPressure_time = tree->Branch("CavernAtmosPressure_time", &cavernAtmosPressure_time);
        bCavernAtmosPressure_time->Fill();
        TBranch* bCavernAtmosPressure2 = tree->Branch("CavernAtmosPressure2", &cavernAtmosPressure2);
        bCavernAtmosPressure->Fill();
        TBranch* bCavernAtmosPressure2_time = tree->Branch("CavernAtmosPressure2_time", &cavernAtmosPressure2_time);
        bCavernAtmosPressure_time->Fill();
      } else if (object == "HV") {
      } else if (object == "Gas") {
        path = CDBTypeMap.at(CDBType::CalGas);
        std::vector<long> times = getTimestamps(mCDB, path, period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for Gas." << std::endl;

        ROOT::RVec<dcs::Gas> gas;
        for (int i = 0; i < nTimes; i++) {
          std::cout << "Processed: " << i << " out of: " << nTimes << std::endl;
          long timestamp = times[i];
          auto gasTmp = mCDB.retrieveFromTFileAny<o2::tpc::dcs::Gas>(path, metadata, timestamp);
          gas.emplace_back(*gasTmp);
          LOGP(info, "Entries gas: {}", gasTmp->neon.data.size());
          // gasTmp -> neon.data[0].value;
          // gasTmp -> neon.data[0].time;
        }

        TBranch* bGas = tree->Branch("Gas", &gas);
        bGas->Fill();
      } else if (object == "VDrift") {
        path = CDBTypeMap.at(CDBType::CalVDriftTgl);
        std::vector<long> times = getTimestamps(mCDB, path, period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for Gas." << std::endl;

        ROOT::RVec<VDriftCorrFact> drift;
        for (int i = 0; i < nTimes; i++) {
          std::cout << "Processed: " << i << " out of: " << nTimes << std::endl;
          long timestamp = times[i];
          auto driftTmp = mCDB.retrieveFromTFileAny<o2::tpc::VDriftCorrFact>(path, metadata, timestamp);
          drift.emplace_back(*driftTmp);
        }
        TBranch* bDrift = tree->Branch("VDrift", &drift);
        bDrift->Fill();
      } else if (object == "TimeGain") {
      } else if (object == "Ltr") {
      } else if (object == "PadGain") {
        path = CDBTypeMap.at(CDBType::CalPadGainFull);

        CalDet<float> padGain;
        std::vector<float> padGainVec;

        padGain = *mCDB.retrieveFromTFileAny<CalDet<float>>(path, metadata, 1);
        padGainVec = calDetToVec(padGain);
        ROOT::RVec<float> padGainRVec(padGainVec.data(), padGainVec.size());

        TBranch* bPadGain = tree->Branch("GainMap", &padGainRVec);
        bPadGain->Fill();
        // std::cout << "Adding " << nTimes << " calDets from padGain to be processed." << std::endl;
      } else if (object == "PadGainResidual") {
        path = CDBTypeMap.at(CDBType::CalPadGainResidual);
        std::vector<long> times = getTimestamps(mCDB, path, period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for PadGainResidual." << std::endl;

        if (nTimes == 0) {
          continue;
        }

        // TODO these are not processed for now...
        std::vector<CalDet<float>> padResGainRMS;
        std::vector<CalDet<float>> padResGain;

        for (int i = 0; i < nTimes; i++) {
          const auto padGainTmp = mCDB.retrieveFromTFileAny<std::unordered_map<std::string, CalDet<float>>>(path, metadata, times[i]);
          for (const auto& map : *padGainTmp) {
            const std::string key = map.first;
            LOGP(info, "Reading in map {}", key);
            if (key == "GainMap") {
              padResGain.emplace_back(map.second);
              padResGain.back().setName("GainMapTracks");
              // set name
            } else if (key == "SigmaMap") {
              padResGainRMS.emplace_back(map.second);
            }
          }
        }
        calDetMatrix.push_back(std::make_pair(padResGain, times));
      }
    }
  }

  // Processing of the downloaded caldet objects.
  for (auto& calDetPair : calDetMatrix) {
    // Generate time series
    std::vector<std::string> confTimeSeries = config["exportTimeSeries"];
    if (!confTimeSeries.empty() || std::find(confTimeSeries.begin(), confTimeSeries.end(), "False") != confTimeSeries.end()) {
      std::vector<long> timeVec(calDetPair.second.begin(), calDetPair.second.end());
      ROOT::RVec<long> timeRVec(timeVec.data(), timeVec.size());

      ROOT::RVec<ROOT::RVec<long>> timeRVecSec;
      ROOT::RVec<ROOT::RVec<long>> timeRVecROC;
      for (int i = 0; i < NSECTORS; ++i) {
        timeRVecSec.emplace_back(timeRVec);
      }
      for (int i = 0; i < NROCS; ++i) {
        timeRVecROC.emplace_back(timeRVec);
      }

      TBranch* bTime = tree->Branch((calDetPair.first.at(0).getName() + "_time").c_str(), &timeRVec);
      bTime->Fill();

      TBranch* bTimeSec = tree->Branch((calDetPair.first.at(0).getName() + "_time_Sec").c_str(), &timeRVecSec);
      bTimeSec->Fill();

      TBranch* bTimeRoc = tree->Branch((calDetPair.first.at(0).getName() + "_time_ROC").c_str(), &timeRVecROC);
      bTimeRoc->Fill();

      if (std::find(confTimeSeries.begin(), confTimeSeries.end(), "Mean") != confTimeSeries.end()) {
        std::vector<float> meanSeries = getTimeSeriesMean(calDetPair.first);
        auto meanSeriesSector = getTimeSeriesMeanSector(calDetPair.first);
        auto meanSeriesROC = getTimeSeriesMeanROC(calDetPair.first);
        ROOT::RVec<float> meanRVec(meanSeries.data(), meanSeries.size());
        ROOT::RVec<ROOT::RVec<float>> meanRVecSector;
        ROOT::RVec<ROOT::RVec<float>> meanRVecROC;
        for (auto& meanSec : meanSeriesSector) {
          meanRVecSector.emplace_back(ROOT::RVec<float>(meanSec.data(), meanSec.size()));
        }
        for (auto& meanROC : meanSeriesROC) {
          meanRVecROC.emplace_back(ROOT::RVec<float>(meanROC.data(), meanROC.size()));
        }
        TBranch* bMean = tree->Branch((calDetPair.first.at(0).getName() + "_Mean_TS").c_str(), "RVec<float>", &meanRVec);
        TBranch* bMeanSec = tree->Branch((calDetPair.first.at(0).getName() + "_Mean_TS_Sec").c_str(), &meanRVecSector);
        TBranch* bMeanROC = tree->Branch((calDetPair.first.at(0).getName() + "_Mean_TS_ROC").c_str(), &meanRVecROC);
        bMean->Fill();
        bMeanSec->Fill();
        bMeanROC->Fill();
      }
      if (std::find(confTimeSeries.begin(), confTimeSeries.end(), "Median") != confTimeSeries.end()) {
        std::vector<float> medianSeries = getTimeSeriesMedian(calDetPair.first);
        auto medianSeriesSector = getTimeSeriesMedianSector(calDetPair.first);
        auto medianSeriesROC = getTimeSeriesMedianROC(calDetPair.first);
        ROOT::RVec<float> medianRVec(medianSeries.data(), medianSeries.size());
        ROOT::RVec<ROOT::RVec<float>> medianRVecSector;
        ROOT::RVec<ROOT::RVec<float>> medianRVecROC;
        for (auto& medianSec : medianSeriesSector) {
          medianRVecSector.emplace_back(ROOT::RVec<float>(medianSec.data(), medianSec.size()));
        }
        for (auto& medianROC : medianSeriesROC) {
          medianRVecROC.emplace_back(ROOT::RVec<float>(medianROC.data(), medianROC.size()));
        }
        TBranch* bMedian = tree->Branch((calDetPair.first.at(0).getName() + "_Median_TS").c_str(), &medianRVec);
        TBranch* bMedianSec = tree->Branch((calDetPair.first.at(0).getName() + "_Median_TS_Sec").c_str(), &medianRVecSector);
        TBranch* bMedianROC = tree->Branch((calDetPair.first.at(0).getName() + "_Median_TS_ROC").c_str(), &medianRVecROC);
        bMedian->Fill();
        bMedianSec->Fill();
        bMedianROC->Fill();
      }
      if (std::find(confTimeSeries.begin(), confTimeSeries.end(), "RMS") != confTimeSeries.end()) {
        std::vector<float> rmsSeries = getTimeSeriesRMS(calDetPair.first);
        auto rmsSeriesSector = getTimeSeriesRMSSector(calDetPair.first);
        auto rmsSeriesROC = getTimeSeriesRMSROC(calDetPair.first);
        ROOT::RVec<float> rmsRVec(rmsSeries.data(), rmsSeries.size());
        ROOT::RVec<ROOT::RVec<float>> rmsRVecSector;
        ROOT::RVec<ROOT::RVec<float>> rmsRVecROC;
        for (auto& rmsSec : rmsSeriesSector) {
          rmsRVecSector.emplace_back(ROOT::RVec<float>(rmsSec.data(), rmsSec.size()));
        }
        for (auto& rmsROC : rmsSeriesROC) {
          rmsRVecROC.emplace_back(ROOT::RVec<float>(rmsROC.data(), rmsROC.size()));
        }
        TBranch* bRMS = tree->Branch((calDetPair.first.at(0).getName() + "_RMS_TS").c_str(), &rmsRVec);
        TBranch* bRMSSec = tree->Branch((calDetPair.first.at(0).getName() + "_RMS_TS_Sec").c_str(), &rmsRVecSector);
        TBranch* bRMSROC = tree->Branch((calDetPair.first.at(0).getName() + "_RMS_TS_ROC").c_str(), &rmsRVecROC);
        bRMS->Fill();
        bRMSSec->Fill();
        bRMSROC->Fill();
      }
    }

    // Create the summary CalDet with pad-wise Mean, Median or RMS or dump the RAW values
    std::vector<float> outVecMean, outVecMedian, outVecRMS, outVec;
    std::vector<std::string> confPerPad = config["exportPerPad"];
    if (!confPerPad.empty() || std::find(confTimeSeries.begin(), confTimeSeries.end(), "False") != confTimeSeries.end()) {
      std::cout << "Processing per pad overviews of obtained CalDet objects" << std::endl;
      if (std::find(confPerPad.begin(), confPerPad.end(), "Mean") != confPerPad.end()) {
        CalDet<float> outCalDetMean;
        getMeanCalDets(outCalDetMean, calDetPair.first);
        std::string nameBr = calDetPair.first.at(0).getName() + "_Mean";
        outVecMean = calDetToVec(outCalDetMean);
        ROOT::RVec<float> outRVecMean(outVecMean.data(), outVecMean.size());
        TBranch* bMean = tree->Branch(nameBr.c_str(), &outRVecMean);
        bMean->Fill();
      }
      if (std::find(confPerPad.begin(), confPerPad.end(), "Median") != confPerPad.end()) {
        CalDet<float> outCalDetMedian;
        getMedianCalDets(outCalDetMedian, calDetPair.first);
        std::string nameBr = calDetPair.first.at(0).getName() + "_Median";
        outVecMedian = calDetToVec(outCalDetMedian);
        ROOT::RVec<float> outRVecMedian(outVecMedian.data(), outVecMedian.size());
        TBranch* bMedian = tree->Branch(nameBr.c_str(), &outRVecMedian);
        bMedian->Fill();
      }
      if (std::find(confPerPad.begin(), confPerPad.end(), "RMS") != confPerPad.end()) {
        CalDet<float> outCalDetRMS;
        getRMSCalDets(outCalDetRMS, calDetPair.first);
        std::string nameBr = calDetPair.first.at(0).getName() + "_RMS";
        outVecRMS = calDetToVec(outCalDetRMS);
        ROOT::RVec<float> outRVecRMS(outVecRMS.data(), outVecRMS.size());
        TBranch* bRMS = tree->Branch(nameBr.c_str(), &outRVecRMS);
        bRMS->Fill();
      }
      if (std::find(confPerPad.begin(), confPerPad.end(), "All") != confPerPad.end()) {
        std::string nameBr = calDetPair.first.at(0).getName();
        ROOT::RVec<float> outRVec(outVec.data(), outVec.size());
        TBranch* bAll = tree->Branch(nameBr.c_str(), &outRVec);
        for (auto& calDet : calDetPair.first) {
          outVec = calDetToVec(calDet);
          outRVec = outVec;
          bAll->Fill();
        }
      }
    }
  }
  // The number of entries have to be set if the TTree was filled by the individual branches (ToDo: Set correct number of entries 1-one config file)
  tree->SetEntries(1);
  return;
}

/// @@@@@@@@@@@@@@@@@@@ Utility @@@@@@@@@@@@@@@@@@@
/// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

std::map<std::string, std::vector<std::string>> getConfiguration(const std::string configFile)
{
  // some default values as an example
  std::map<std::string, std::vector<std::string>> config{
    {"input", {}},
    {"object", {}},
    {"runNumber", {}},
    {"normalizePerPad", {}},
    {"exportPerPad", {}},
    {"exportTimeSeries", {}}};
  boost::property_tree::ptree pt;
  read_json(configFile, pt);

  for (auto& myPair : config) {
    for (boost::property_tree::ptree::value_type& myData : pt.get_child(myPair.first)) {
      myPair.second.push_back(myData.second.data());
    }
  }
  return config;
}

std::vector<std::map<std::string, std::vector<std::string>>> getConfigurations(const std::string configFile)
{
  // some default values as an example
  std::map<std::string, std::vector<std::string>> config{
    {"input", {}},
    {"object", {}},
    {"runNumber", {}},
    {"timeRange", {}},
    {"normalizePerPad", {}},
    {"exportPerPad", {}},
    {"exportTimeSeries", {}}};

  std::vector<std::map<std::string, std::vector<std::string>>> configs;
  boost::property_tree::ptree pt;
  read_json(configFile, pt);

  std::vector<std::string> keys;
  for (auto const& imap : config)
    keys.push_back(imap.first);

  for (boost::property_tree::ptree::value_type& myConfig : pt.get_child("configs")) {
    assert(myConfig.first.empty()); // array elements have no names
    std::cout << "First config" << std::endl;
    config = {
      {"input", {}},
      {"object", {}},
      {"runNumber", {}},
      {"timeRange", {}},
      {"normalizePerPad", {}},
      {"exportPerPad", {}},
      {"exportTimeSeries", {}}};
    ptree::const_iterator end = myConfig.second.end();
    for (ptree::const_iterator it = myConfig.second.begin(); it != end; ++it) {
      std::string setting;
      if (!it->first.empty()) {
        setting = it->first;
        // std::cout << "Setting: " << it->first << std::endl;
        if (std::find(keys.begin(), keys.end(), setting) != keys.end()) {
          std::vector<std::string> values;
          if (!it->second.empty()) {
            ptree::const_iterator end2 = it->second.end();
            for (ptree::const_iterator it2 = it->second.begin(); it2 != end2; ++it2) {
              values.push_back(it2->second.get_value<std::string>());
              // std::cout << "- Value: " << it2->second.get_value<std::string>() << std::endl;
            }
          }
          config[setting] = values;
        } else {
          std::cout << "[ERROR] Provided key " << setting << " doesn't match map" << std::endl;
        }
      }
    }
    configs.push_back(config);
  }
  return configs;
}

std::vector<long> getSorEor(const int run, const long offsStart, const long offsEnd)
{
  // const auto offsStartMS = offsStart * std::pow(10, 3);
  // const auto offsEndMS = offsEnd * std::pow(10, 3);
  o2::ccdb::CcdbApi c;
  c.init("http://alice-ccdb.cern.ch");
  std::map<std::string, std::string> headers, metadataRCT, metadata, mm;

  headers = c.retrieveHeaders(fmt::format("RCT/Info/RunInformation/{}", run), metadataRCT, -1);
  auto sor = std::stol(headers["SOR"].data());
  auto eor = std::stol(headers["EOR"].data());

  headers = c.retrieveHeaders(fmt::format("GLO/Config/GRPECS/{}", sor), mm, -1);
  const auto eor2 = std::stol(headers["Valid-Until"].data());
  const auto sor2 = std::stol(headers["Valid-From"].data());

  if (eor == 0) {
    eor = eor2;
  }

  sor += offsStart * std::pow(10, 3);
  if (offsEnd > 0) {
    long eorTmp = sor + offsEnd * std::pow(10, 3);
    if (eorTmp < eor) {
      eor = eorTmp;
    }
  }

  // abort if the start of run is after end of run
  assert(sor > eor);

  std::cout << "Run " << run << " ==> SOR: " << sor << " - EOR: " << eor << std::endl;
  std::vector<long> output = {sor, eor};
  return output;
}

std::vector<long> getTimestamps(o2::ccdb::CcdbApi mCDB, const std::string path, const std::vector<long> period)
{
  std::vector<long> outVec;
  for (size_t run = 0; run < period.size() / 2; run++) {
    long sor = period[0 + (run * 2)];
    long eor = period[1 + (run * 2)];
    o2::quality_control_modules::tpc::DCSPTemperature DCSPtools;
    std::vector<std::string> fileList = DCSPtools.splitString(mCDB.list(path), "\n");
    if (fileList.size()) {
      for (auto& file : fileList) {
        long timestamp = DCSPtools.getTimestamp(file);
        if (timestamp <= eor && timestamp >= sor) {
          outVec.emplace_back(timestamp);
          std::cout << "timestamp: " << timestamp << std::endl;
        }
      }
    } else
      std::cout << "[ERROR] No files found in given CDB directory: " << mCDB.list(path) << std::endl;
  }
  std::sort(outVec.begin(), outVec.end());
  return outVec;
}

float getMeanCalDet(CalDet<float>& calDet)
{
  float out;
  std::vector<float> pads;
  for (auto& calArray : calDet.getData()) {
    std::vector<float> data = calArray.getData();
    pads.insert(pads.end(), data.begin(), data.end());
  }
  out = TMath::Mean(pads.begin(), pads.end());
  return out;
}
float getMedianCalDet(CalDet<float>& calDet)
{
  float out;
  std::vector<float> pads;
  for (auto& calArray : calDet.getData()) {
    std::vector<float> data = calArray.getData();
    pads.insert(pads.end(), data.begin(), data.end());
  }
  out = TMath::Median(pads.size(), pads.data());
  return out;
}
float getRMSCalDet(CalDet<float>& calDet)
{
  float out;
  std::vector<float> pads;
  for (auto& calArray : calDet.getData()) {
    std::vector<float> data = calArray.getData();
    pads.insert(pads.end(), data.begin(), data.end());
  }
  out = TMath::RMS(pads.size(), pads.data());
  return out;
}

void getMeanCalDets(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects)
{
  // CalDet<float> out;
  out = (float)0.;
  for (auto& calDet : mCalDetObjects) {
    out += calDet;
  }
  out /= static_cast<float>(mCalDetObjects.size());
  return;
}

void getMedianCalDets(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects)
{
  out = (float)0.;
  uint16_t rocNumber = 0;
  int ntime = mCalDetObjects.size();
  float padVal[ntime];

  auto& outData = out.getData();
  for (size_t i = 0; i < outData.size(); i++) {
    auto& vals = outData[i].getData();
    std::vector<std::vector<float>> invals;
    for (int itime = 0; itime < ntime; itime++) {
      auto inData = mCalDetObjects[itime].getData();
      invals.push_back(inData[i].getData());
    }
    for (int j = 0; j < (int)vals.size(); j++) {
      for (int itime = 0; itime < ntime; itime++) {
        padVal[itime] = invals[itime][j];
      }
      vals[j] = TMath::Median(ntime, padVal);
    }
  }
  return;
}

void getRMSCalDets(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects)
{
  out = (float)0.;
  uint16_t rocNumber = 0;
  int ntime = mCalDetObjects.size();
  float padVal[ntime];

  auto& outData = out.getData();
  for (size_t i = 0; i < outData.size(); i++) {
    auto& vals = outData[i].getData();
    std::vector<std::vector<float>> invals;
    for (int itime = 0; itime < ntime; itime++) {
      auto inData = mCalDetObjects[itime].getData();
      invals.push_back(inData[i].getData());
    }
    for (int j = 0; j < (int)vals.size(); j++) {
      for (int itime = 0; itime < ntime; itime++) {
        padVal[itime] = invals[itime][j];
      }
      vals[j] = TMath::RMS(ntime, padVal);
    }
  }
  return;
}

void normalizeCalDets(std::vector<CalDet<float>>& mCalDetObjects)
{
  std::vector<float> pads;
  for (auto& calDet : mCalDetObjects) {
    float sum = 0;
    auto& calDetdata = calDet.getData();
    for (auto& calArray : calDetdata) {
      auto& data = calArray.getData();
      for (auto&& pad : data) {
        sum += pad;
      }
    }
    if (sum != 0) {
      for (auto& calArray : calDetdata) {
        auto& data = calArray.getData();
        for (auto&& pad : data) {
          pad /= sum;
        }
      }
    }
  }
}

void normalizeCalDet(CalDet<float>& calDet)
{
  std::vector<float> pads;
  float sum = 0;
  auto& calDetdata = calDet.getData();
  for (auto& calArray : calDetdata) {
    auto& data = calArray.getData();
    for (auto&& pad : data) {
      sum += pad;
    }
  }
  if (sum != 0) {
    for (auto& calArray : calDetdata) {
      auto& data = calArray.getData();
      for (auto&& pad : data) {
        pad /= sum;
      }
    }
  }
}

std::vector<float> getTimeSeriesMean(const std::vector<CalDet<float>>& mCalDetObjects, float min, float max)
{
  std::vector<float> out;
  std::vector<float> pads;
  for (auto& calDet : mCalDetObjects) {
    pads.clear();
    for (auto& calArray : calDet.getData()) {
      std::vector<float> data = calArray.getData();

      // std::cout << "data size before: " << data.size() << std::endl;
      // data.erase(std::remove_if(data.begin(), data.end(), [min, max](const float x) { return (x < min || x > max); }), data.end());
      // std::cout << "data size after: " << data.size() << std::endl;

      pads.insert(pads.end(), data.begin(), data.end());
    }
    out.push_back(TMath::Mean(pads.begin(), pads.end()));
  }
  return out;
}

std::array<std::vector<float>, NSECTORS> getTimeSeriesMeanSector(const std::vector<CalDet<float>>& mCalDetObjects)
{
  std::array<std::vector<float>, NSECTORS> out;
  for (auto& calDet : mCalDetObjects) {
    std::array<std::vector<float>, NSECTORS> pads;
    for (auto& calArray : calDet.getData()) {
      ROC roc(calArray.getPadSubsetNumber());
      int sec = roc.getSector();
      std::vector<float> data = calArray.getData();
      pads[sec].insert(pads[sec].end(), data.begin(), data.end());
    }
    for (int isec = 0; isec < NSECTORS; ++isec) {
      out[isec].push_back(TMath::Mean(pads[isec].size(), pads[isec].data()));
    }
  }
  return out;
}

std::array<std::vector<float>, NROCS> getTimeSeriesMeanROC(const std::vector<CalDet<float>>& mCalDetObjects)
{
  std::array<std::vector<float>, NROCS> out;
  for (auto& calDet : mCalDetObjects) {
    std::array<std::vector<float>, NROCS> pads;
    for (auto& calArray : calDet.getData()) {
      int roc = calArray.getPadSubsetNumber();
      std::vector<float> data = calArray.getData();
      pads[roc].insert(pads[roc].end(), data.begin(), data.end());
    }
    for (int iroc = 0; iroc < NROCS; ++iroc) {
      out[iroc].push_back(TMath::Mean(pads[iroc].size(), pads[iroc].data()));
    }
  }
  return out;
}

std::vector<float> getTimeSeriesMedian(const std::vector<CalDet<float>>& mCalDetObjects)
{
  std::vector<float> out;
  std::vector<float> pads;
  for (auto& calDet : mCalDetObjects) {
    pads.clear();
    for (auto& calArray : calDet.getData()) {
      std::vector<float> data = calArray.getData();
      pads.insert(pads.end(), data.begin(), data.end());
    }
    out.push_back(TMath::Median(pads.size(), pads.data()));
  }
  return out;
}

std::array<std::vector<float>, NSECTORS> getTimeSeriesMedianSector(const std::vector<CalDet<float>>& mCalDetObjects)
{
  std::array<std::vector<float>, NSECTORS> out;
  for (auto& calDet : mCalDetObjects) {
    std::array<std::vector<float>, NSECTORS> pads;
    for (auto& calArray : calDet.getData()) {
      ROC roc(calArray.getPadSubsetNumber());
      int sec = roc.getSector();
      std::vector<float> data = calArray.getData();
      pads[sec].insert(pads[sec].end(), data.begin(), data.end());
    }
    for (int isec = 0; isec < NSECTORS; ++isec) {
      out[isec].push_back(TMath::Median(pads[isec].size(), pads[isec].data()));
    }
  }
  return out;
}

std::array<std::vector<float>, NROCS> getTimeSeriesMedianROC(const std::vector<CalDet<float>>& mCalDetObjects)
{
  std::array<std::vector<float>, NROCS> out;
  for (auto& calDet : mCalDetObjects) {
    std::array<std::vector<float>, NROCS> pads;
    for (auto& calArray : calDet.getData()) {
      int roc = calArray.getPadSubsetNumber();
      std::vector<float> data = calArray.getData();
      pads[roc].insert(pads[roc].end(), data.begin(), data.end());
    }
    for (int iroc = 0; iroc < NROCS; ++iroc) {
      out[iroc].push_back(TMath::Median(pads[iroc].size(), pads[iroc].data()));
    }
  }
  return out;
}

std::vector<float> getTimeSeriesRMS(const std::vector<CalDet<float>>& mCalDetObjects)
{
  std::vector<float> out;
  std::vector<float> pads;
  for (auto& calDet : mCalDetObjects) {
    pads.clear();
    for (auto& calArray : calDet.getData()) {
      std::vector<float> data = calArray.getData();
      pads.insert(pads.end(), data.begin(), data.end());
    }
    out.push_back(TMath::RMS(pads.size(), pads.data()));
  }
  return out;
}

std::array<std::vector<float>, NSECTORS> getTimeSeriesRMSSector(const std::vector<CalDet<float>>& mCalDetObjects)
{
  std::array<std::vector<float>, NSECTORS> out;
  for (auto& calDet : mCalDetObjects) {
    std::array<std::vector<float>, NSECTORS> pads;
    for (auto& calArray : calDet.getData()) {
      ROC roc(calArray.getPadSubsetNumber());
      int sec = roc.getSector();
      std::vector<float> data = calArray.getData();
      pads[sec].insert(pads[sec].end(), data.begin(), data.end());
    }
    for (int isec = 0; isec < NSECTORS; ++isec) {
      out[isec].push_back(TMath::RMS(pads[isec].size(), pads[isec].data()));
    }
  }
  return out;
}

std::array<std::vector<float>, NROCS> getTimeSeriesRMSROC(const std::vector<CalDet<float>>& mCalDetObjects)
{
  std::array<std::vector<float>, NROCS> out;
  for (auto& calDet : mCalDetObjects) {
    std::array<std::vector<float>, NROCS> pads;
    for (auto& calArray : calDet.getData()) {
      int roc = calArray.getPadSubsetNumber();
      std::vector<float> data = calArray.getData();
      pads[roc].insert(pads[roc].end(), data.begin(), data.end());
    }
    for (int iroc = 0; iroc < NROCS; ++iroc) {
      out[iroc].push_back(TMath::RMS(pads[iroc].size(), pads[iroc].data()));
    }
  }
  return out;
}

std::vector<float> calDetToVec(const CalDet<float>& calDet)
{
  std::vector<float> pads;
  float sum = 0;
  auto& calDetdata = calDet.getData();
  for (auto& calArray : calDetdata) {
    auto& data = calArray.getData();
    pads.insert(pads.end(), data.begin(), data.end());
  }
  return pads;
}

void addMappingInfo(TTree* tree)
{
  const auto& mapper = Mapper::instance();
  auto mTraceLengthIROC = mapper.getTraceLengthsIROC();
  auto mTraceLengthOROC = mapper.getTraceLengthsOROC();
  // ===| default mapping objects |=============================================
  ROOT::RVec<int> rocNumber;
  // positions
  ROOT::RVec<float> gx, gy, lx, ly;
  // row and pad
  ROOT::RVec<unsigned char> row, pad, padrow;
  ROOT::RVec<short> cpad;
  ROOT::RVec<unsigned char> fecInSector, sampaOnFEC, channelOnSampa;
  ROOT::RVec<float> traceLength;

  // ===| add branches with default mappings |==================================
  tree->Branch("roc", &rocNumber);
  tree->Branch("gx", &gx);
  tree->Branch("gy", &gy);
  tree->Branch("lx", &lx);
  tree->Branch("ly", &ly);
  tree->Branch("row", &row);
  tree->Branch("pad", &pad);
  tree->Branch("padrow", &padrow);
  tree->Branch("cpad", &cpad);
  tree->Branch("fecInSector", &fecInSector);
  tree->Branch("sampaOnFEC", &sampaOnFEC);
  tree->Branch("channelOnSampa", &channelOnSampa);
  tree->Branch("traceLength", &traceLength);

  // fill meta data
  ROOT::RVec<int> sector(NSECTORS);
  std::iota(sector.begin(), sector.end(), 0);
  TBranch* bSec = tree->Branch("Sec", &sector);

  ROOT::RVec<int> rocs(NROCS);
  std::iota(rocs.begin(), rocs.end(), 0);
  TBranch* bRoc = tree->Branch("ROC", &rocs);

  ROOT::RVec<int> stack(GEMSTACKS);
  std::iota(stack.begin(), stack.end(), 0);
  TBranch* bStack = tree->Branch("Stack", &stack);

  // ===| loop over readout chambers |==========================================
  for (ROC roc; !roc.looped(); ++roc) {
    auto tmpTraceLength = (roc.rocType() == RocType::IROC) ? &mTraceLengthIROC : &mTraceLengthOROC;
    traceLength.insert(traceLength.end(), tmpTraceLength->begin(), tmpTraceLength->end());
    // ===| loop over pad rows |================================================
    const int numberOfRows = mapper.getNumberOfRowsROC(roc);
    for (int irow = 0; irow < numberOfRows; ++irow) {
      // ===| loop over pads in row |===========================================
      const int numberOfPadsInRow = mapper.getNumberOfPadsInRowROC(roc, irow);
      for (int ipad = 0; ipad < numberOfPadsInRow; ++ipad) {
        const PadROCPos padROCPos(roc, irow, ipad);
        const PadPos padPos = mapper.getGlobalPadPos(padROCPos); // pad and row in sector
        const PadCentre& localPadXY = mapper.getPadCentre(padPos);
        const LocalPosition2D globalPadXY = mapper.getPadCentre(padROCPos);
        const auto& fecInfo = mapper.getFECInfo(padROCPos);

        rocNumber.emplace_back(roc);
        gx.emplace_back(globalPadXY.X());
        gy.emplace_back(globalPadXY.Y());
        lx.emplace_back(localPadXY.X());
        ly.emplace_back(localPadXY.Y());
        row.emplace_back(irow);
        pad.emplace_back(ipad);
        padrow.emplace_back((roc.rocType() == RocType::IROC) ? irow : irow + mapper.getNumberOfRowsInIROC());
        cpad.emplace_back(ipad - numberOfPadsInRow / 2);
        fecInSector.emplace_back(fecInfo.getIndex());
        sampaOnFEC.emplace_back(fecInfo.getSampaChip());
        channelOnSampa.emplace_back(fecInfo.getSampaChannel());
      }
    }
  }
  tree->Fill();
}

void setDefaultAliases(TTree* tree)
{
  tree->SetAlias("sector", "roc%36");
  tree->SetAlias("padsPerRow", "2*(pad-cpad)");
  tree->SetAlias("isEdgePad", "(pad==0) || (pad==padsPerRow-1)");
  tree->SetAlias("rowInSector", "row + (roc>35)*63");
  tree->SetAlias("padWidth", "0.4 + (roc > 35) * 0.2");
  tree->SetAlias("padHeight", "0.75 + (rowInSector > 62) * 0.25 + (rowInSector > 96) * 0.2 + (rowInSector > 126) * 0.3");
  tree->SetAlias("padArea", "padWidth * padHeight");

  tree->SetAlias("cruInSector", "(rowInSector >= 17) + (rowInSector >= 32) + (rowInSector >= 48) + (rowInSector >= 63) + (rowInSector >= 81) + (rowInSector >= 97) + (rowInSector >= 113) + (rowInSector >= 127) + (rowInSector >= 140)");
  tree->SetAlias("cruID", "cruInSector + sector*10");
  tree->SetAlias("region", "cruInSector");
  tree->SetAlias("partition", "int(cruInSector / 2)");

  tree->SetAlias("padWidth", "(region == 0) * 0.416 + (region == 1) * 0.42 + (region == 2) * 0.42 + (region == 3) * 0.436 + (region == 4) * 0.6 + (region == 5) * 0.6 + (region == 6) * 0.608 + (region == 7) * 0.588 + (region == 8) * 0.604 + (region == 9) * 0.607");
  tree->SetAlias("padHeight", "0.75 + (region>3)*0.25 + (region>5)*0.2 + (region>7)*0.3");
  tree->SetAlias("padArea", "padHeight * padWidth");

  tree->SetAlias("IROC", "roc < 36");
  tree->SetAlias("OROC", "roc >= 36");
  tree->SetAlias("OROC1", "partition == 2");
  tree->SetAlias("OROC2", "partition == 3");
  tree->SetAlias("OROC3", "partition == 4");

  tree->SetAlias("A_Side", "sector < 18");
  tree->SetAlias("C_Side", "sector >= 18");

  tree->SetAlias("fecID", "fecInSector + sector * 91");
  tree->SetAlias("sampaInSector", "sampaOnFEC + 5 * fecInSector");
  tree->SetAlias("channelOnFEC", "channelOnSampa + 32 * sampaOnFEC");
  tree->SetAlias("channelInSector", "channelOnFEC + 160 * fecInSector");
}

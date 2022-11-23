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

#include "TPCCalibration/CalibTreeDump.h"
#include "TPCCalibration/IDCContainer.h"
#include "TPCCalibration/IDCGroupHelperSector.h"
#include "TPCCalibration/IDCCCDBHelper.h"
#include "TPCCalibration/SACCCDBHelper.h"

#include "QualityControl/TPC/ClustersData.h"
#include "QualityControl/TPC/DCSPTemperature.h"


using namespace o2::tpc;
using boost::property_tree::ptree;
using QCCL = o2::quality_control_modules::tpc::ClustersData;
using dataT = unsigned char;

// Function to extract configuration from config file
std::map<std::string, std::vector<std::string>> getConfiguration (const std::string configFile);
std::vector<std::map<std::string, std::vector<std::string>>> getConfigurations (const std::string configFile);

// Function to add objects to file
void addObjects(std::map<std::string, std::vector<std::string>> config, TTree* tree);

// Function to get the End-Of-Run and Start-Of-Run timestamps for given run 
std::vector<long> getSorEor(const int run);

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
std::vector<float> getTimeSeriesMean(const std::vector<CalDet<float>>& mCalDetObjects);
std::vector<float> getTimeSeriesMedian(const std::vector<CalDet<float>>& mCalDetObjects);
std::vector<float> getTimeSeriesRMS(const std::vector<CalDet<float>>& mCalDetObjects);

o2::ccdb::CcdbApi mCDB;

// Main
void tpcQCIDC(const std::string configFile)
{
  std::string filename = "out.root";
  TFile* file(TFile::Open(filename.c_str(), "recreate"));
  TTree *tree = new TTree("QCtree","Tree containing QC observables");
  // Read config
  auto configs = getConfigurations(configFile);

  for (auto& config : configs) {
    std::cout << "Using config file: " << configFile << ". Read options: " << std::endl;
    for ( const auto &myPair : config ) std::cout << myPair.first << " : " << myPair.second.at(0) <<  std::endl;

    // Get and add all objects according to config
    addObjects(config, tree);
  }
  file->cd();
  tree->Write();
  file->Close();
  return;
}

void addObjects(std::map<std::string,std::vector<std::string>> config, TTree* tree)
{ 

  std::vector<std::pair<std::vector<CalDet<float>>,std::vector<long>>> calDetMatrix;

  // Retrieve all CalDet objects
  for (size_t iObject=0; iObject < config["object"].size(); iObject++){
    std::string object = config["object"].at(iObject);
    std::cout << "Will get objects for " << object << std::endl;

    if (mCDB.getURL() != config["input"].at(iObject)) {
      std::cout << "Accessing new DB @ " << config["input"].at(iObject) << std::endl;
      mCDB.init(config["input"].at(iObject));
    }
    std::map<std::string, std::string> metadata;

    std::vector<long> period;
    for (size_t iRun=0; iRun < config["runNumber"].size(); iRun++){
      int run = std::stoi(config["runNumber"].at(iRun));
      std::vector<long> sorEor = getSorEor(run);
      period.insert(period.end(), sorEor.begin(), sorEor.end());
      std::cout << "Looking at run: " << run << " for object: " << object << std::endl;
  
      std::string path;

      if(object == "Clusters") {
        path = "qc/TPC/MO/Clusters/ClusterData";
        std::vector<long> times = getTimestamps(mCDB, path , period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for Clusters." << std::endl;

        long timestamp;
        std::vector<CalDet<float>> clusterNClusters(nTimes);
        std::vector<CalDet<float>> clusterQMax(nTimes);
        std::vector<CalDet<float>> clusterQTot(nTimes);
        std::vector<CalDet<float>> clusterSigmaTime(nTimes); 
        std::vector<CalDet<float>> clusterSigmaPad(nTimes);
        std::vector<CalDet<float>> clusterTimeBin(nTimes);
        
        for (int i=0; i<nTimes; i++) {
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

        calDetMatrix.push_back(std::make_pair(clusterNClusters,times));
        calDetMatrix.push_back(std::make_pair(clusterQMax,times));
        calDetMatrix.push_back(std::make_pair(clusterQTot,times));
        calDetMatrix.push_back(std::make_pair(clusterSigmaTime,times));
        calDetMatrix.push_back(std::make_pair(clusterSigmaPad,times));
        calDetMatrix.push_back(std::make_pair(clusterTimeBin,times));
        
        std::cout << "Adding " << nTimes << " calDets from ClusterData to be processed." << std::endl;
      }
      else if(object == "RawDigits") {
        path = "qc/TPC/MO/RawDigits/RawDigitData";
        std::vector<long> times = getTimestamps(mCDB, path , period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for RawDigits." << std::endl;

        long timestamp;
        std::vector<CalDet<float>> digitNClusters(nTimes);
        std::vector<CalDet<float>> digitQMax(nTimes);
        
        for (int i=0; i<nTimes; i++) {
          long timestamp = times[i];
          auto qccl = mCDB.retrieveFromTFileAny<QCCL>(path, metadata, timestamp);
          auto& clInfo = qccl->getClusters();
          digitNClusters[i] = clInfo.getNClusters();
          digitNClusters[i].setName("NClusters_Digits");
          digitQMax[i] = clInfo.getQMax();
          digitQMax[i].setName("QMax_Digits");
        }

        calDetMatrix.push_back(std::make_pair(digitNClusters,times));
        calDetMatrix.push_back(std::make_pair(digitQMax,times));

        std::cout << "Adding " << nTimes << " calDets from RawDigitData to be processed." << std::endl;
      }
      else if(object == "IDCZero") {
        path = "TPC/Calib/IDC_0_A";
        std::vector<long> times = getTimestamps(mCDB, path, period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for IDC_0." << std::endl;

        long timestamp;
        IDCCCDBHelper<dataT> helper;
        std::vector<CalDet<float>> idcZeros(nTimes);

        for (int i=0; i<nTimes; i++) {
          long timestamp = times[i];
          auto mIDCZeroA = mCDB.retrieveFromTFileAny<o2::tpc::IDCZero>(path, metadata, timestamp);
          auto mIDCZeroC = mCDB.retrieveFromTFileAny<o2::tpc::IDCZero>("TPC/Calib/IDC_0_C", metadata, timestamp);

          helper.setIDCZero(mIDCZeroA, Side::A);
          helper.setIDCZero(mIDCZeroC, Side::C);      
          helper.createOutlierMap(); // create outlier map for the IDC0 which are currently set

          const bool rejectOutlier = true;
          float scalingValIDCA = helper.scaleIDC0(Side::A, rejectOutlier);
          float scalingValIDCC = helper.scaleIDC0(Side::C, rejectOutlier);

          idcZeros[i] = helper.getIDCZeroCalDet();
        }
        calDetMatrix.push_back(std::make_pair(idcZeros,times));
      }
      // Need to find a way to convert SAC (value per stack) to CalPad (value per pad)
      else if(object == "SACZero") {
        path = "TPC/Calib/SAC_0";
        std::vector<long> times = getTimestamps(mCDB, path, period);
        int nTimes = times.size();
        std::cout << "Found " << nTimes << " objects matching the config for SAC_0." << std::endl;
      
        long timestamp;
        SACCCDBHelper<dataT> helperSAC;
        CalDet<float> sacZeroCalDet("SAC0");
        std::vector<CalDet<float>> sacZeros(nTimes);

        for (int i=0; i<nTimes; i++) {
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
        calDetMatrix.push_back(std::make_pair(sacZeros,times));
      }

    }
  }

  // Processing of the downloaded caldet objects.
  
  for (auto& calDetPair : calDetMatrix){
    // Generate time series
    std::vector<std::string> confTimeSeries = config["exportTimeSeries"];
    if (!confTimeSeries.empty()) {
      std::vector<float> timeVec(calDetPair.second.begin(), calDetPair.second.end());
      ROOT::RVec<float> timeRVec(timeVec.data(), timeVec.size());
      //file->WriteObject(&timeRVec,(calDetPair.first.at(0).getName()+"_time").c_str());
      tree->Branch((calDetPair.first.at(0).getName()+"_time").c_str(),&timeRVec);
      std::cout << "Processing timeseries of obtained CalDet objects" << std::endl;
      if (std::find(confTimeSeries.begin(),confTimeSeries.end(),"Mean") != confTimeSeries.end()) {
        std::vector<float> meanSeries = getTimeSeriesMean(calDetPair.first);
        ROOT::RVec<float> meanRVec(timeVec.data(), timeVec.size());
        //file->WriteObject(&meanRVec,(calDetPair.first.at(0).getName()+"_timeSeries_Mean").c_str());
        tree->Branch((calDetPair.first.at(0).getName()+"_timeSeries_Mean").c_str(),&meanRVec);
      }
      if (std::find(confTimeSeries.begin(),confTimeSeries.end(),"Median") != confTimeSeries.end()) {
        std::vector<float> medianSeries = getTimeSeriesMedian(calDetPair.first);
        ROOT::RVec<float> medianRVec(timeVec.data(), timeVec.size());
        //file->WriteObject(&medianRVec,(calDetPair.first.at(0).getName()+"_timeSeries_Median").c_str());
        tree->Branch((calDetPair.first.at(0).getName()+"_timeSeries_Median").c_str(),&medianRVec);
      }
      if (std::find(confTimeSeries.begin(),confTimeSeries.end(),"RMS") != confTimeSeries.end()) {
        std::vector<float> rmsSeries = getTimeSeriesRMS(calDetPair.first);
        ROOT::RVec<float> rmsRVec(timeVec.data(), timeVec.size());
        //file->WriteObject(&rmsRVec,(calDetPair.first.at(0).getName()+"_timeSeries_RMS").c_str());
        tree->Branch((calDetPair.first.at(0).getName()+"_timeSeries_RMS").c_str(),&rmsRVec);
      }
    }

    // Create the summary CalDet with pad-wise Mean, Median or RMS
    CalDet<float> outCalDetMean, outCalDetMedian, outCalDetRMS, outCalDet;
    std::vector<std::string> confPerPad = config["exportPerPad"];
    if (!confPerPad.empty()) {
      std::cout << "Processing per pad overviews of obtained CalDet objects" << std::endl;
      if (std::find(confPerPad.begin(),confPerPad.end(),"Mean") != confPerPad.end()) {
        getMeanCalDets(outCalDetMean,calDetPair.first);
        outCalDetMean.setName(calDetPair.first.at(0).getName() + "_Mean");
        //file->WriteObject(&outCalDetMean,outCalDet.getName().c_str());
        tree->Branch(outCalDetMean.getName().c_str(),"CalDet<float>",&outCalDetMean);
      } 
      if (std::find(confPerPad.begin(),confPerPad.end(),"Median") != confPerPad.end()) {
        getMedianCalDets(outCalDetMedian,calDetPair.first);
        outCalDetMedian.setName(calDetPair.first.at(0).getName() + "_Median");
        //file->WriteObject(&outCalDet,outCalDet.getName().c_str());
        tree->Branch(outCalDetMedian.getName().c_str(),"CalDet<float>",&outCalDetMedian);
      }
      if (std::find(confPerPad.begin(),confPerPad.end(),"RMS") != confPerPad.end()) {
        getRMSCalDets(outCalDetRMS,calDetPair.first);
        outCalDetRMS.setName(calDetPair.first.at(0).getName() + "_RMS");
        //file->WriteObject(&outCalDet,outCalDet.getName().c_str());
        tree->Branch(outCalDetRMS.getName().c_str(),"CalDet<float>",&outCalDetRMS);
      }
      if (std::find(confPerPad.begin(),confPerPad.end(),"All") != confPerPad.end()) {
        tree->Branch(calDetPair.first.at(0).getName().c_str(),"CalDet<float>",&outCalDet);
        for (auto& calDet : calDetPair.first) {
          outCalDet = calDet;
          tree->Fill();
        }
      }
    }
    tree->Fill();
  }
  return;
}

/// @@@@@@@@@@@@@@@@@@@ Utility @@@@@@@@@@@@@@@@@@@ 
/// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

std::map<std::string, std::vector<std::string>> getConfiguration (const std::string configFile){
  // some default values as an example
  std::map<std::string, std::vector<std::string>> config{      
    {"input", {}},
    {"object", {}},
    {"runNumber", {}},
    {"normalizePerPad", {}},
    {"exportPerPad", {}},
    {"exportTimeSeries", {}}
  };
  boost::property_tree::ptree pt;
  read_json(configFile, pt);

  for (auto &myPair : config ) {
    for (boost::property_tree::ptree::value_type &myData : pt.get_child(myPair.first)) {
        myPair.second.push_back(myData.second.data());
    }
  }
  return config;
}

std::vector<std::map<std::string, std::vector<std::string>>> getConfigurations (const std::string configFile){
  // some default values as an example
  std::map<std::string, std::vector<std::string>> config{      
    {"input", {}},
    {"object", {}},
    {"runNumber", {}},
    {"normalizePerPad", {}},
    {"exportPerPad", {}},
    {"exportTimeSeries", {}}
  };

  std::vector<std::map<std::string, std::vector<std::string>>> configs;
  boost::property_tree::ptree pt;
  read_json(configFile, pt);

  std::vector<std::string> keys;
  for(auto const& imap: config) keys.push_back(imap.first);

  for (boost::property_tree::ptree::value_type &myConfig : pt.get_child("configs")) {
    assert(myConfig.first.empty()); // array elements have no names
    std::cout << "First config" << std::endl;
    config = {      
      {"input", {}},
      {"object", {}},
      {"runNumber", {}},
      {"normalizePerPad", {}},
      {"exportPerPad", {}},
      {"exportTimeSeries", {}}
    };
    ptree::const_iterator end = myConfig.second.end();
    for (ptree::const_iterator it = myConfig.second.begin(); it != end; ++it) {
      std::string setting;
      if(!it->first.empty()) {
        setting = it->first;
        //std::cout << "Setting: " << it->first << std::endl;
        if (std::find(keys.begin(), keys.end(), setting) != keys.end()) {
          std::vector<std::string> values;
          if(!it->second.empty()) {
            ptree::const_iterator end2 = it->second.end();
            for (ptree::const_iterator it2 = it->second.begin(); it2 != end2; ++it2) {
              values.push_back(it2->second.get_value<std::string>());
              //std::cout << "- Value: " << it2->second.get_value<std::string>() << std::endl;
            } 
          }
          config[setting] = values;
        }
        else {
            std::cout << "[ERROR] Provided key "<< setting <<" doesn't match map" << std::endl;
        } 
      }                                      
    }
    configs.push_back(config);
  }
  return configs;
}

std::vector<long> getSorEor(const int run){
  o2::ccdb::CcdbApi c;
  c.init("http://alice-ccdb.cern.ch");
  std::map<std::string, std::string> headers, metadataRCT, metadata, mm;

  headers = c.retrieveHeaders(fmt::format("RCT/Info/RunInformation/{}", run), metadataRCT, -1);
  const auto sor = std::stol(headers["SOR"].data());
  auto eor = std::stol(headers["EOR"].data());

  headers = c.retrieveHeaders(fmt::format("GLO/Config/GRPECS/{}", sor), mm, -1);
  const auto eor2 = std::stol(headers["Valid-Until"].data());
  const auto sor2 = std::stol(headers["Valid-From"].data());

  if (eor == 0) {
    eor = eor2;
  }
  std::cout << "Run " << run << " ==> SOR: " << sor << " - EOR: " << eor << std::endl;
  std::vector<long> output = {sor,eor};
  return output;
}

std::vector<long> getTimestamps(o2::ccdb::CcdbApi mCDB, const std::string path, const std::vector<long> period)
{ 
  std::vector<long> outVec;
  for(size_t run=0; run<period.size()/2; run++){
    long sor = period[0+(run*2)];
    long eor = period[1+(run*2)];
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
    }
    else std::cout << "[ERROR] No files found in given CDB directory: " << mCDB.list(path) << std::endl;
  }
  return outVec;
}

float getMeanCalDet(CalDet<float>& calDet) {
  float out;
  std::vector<float> pads;
  for (auto& calArray : calDet.getData()) {
    std::vector<float> data = calArray.getData();
    pads.insert( pads.end(), data.begin(), data.end() );
  }
  out = TMath::Mean(pads.begin(),pads.end());
  return out;
}
float getMedianCalDet(CalDet<float>& calDet) {
  float out;
  std::vector<float> pads;
  for (auto& calArray : calDet.getData()) {
    std::vector<float> data = calArray.getData();
    pads.insert( pads.end(), data.begin(), data.end() );
  }
  out = TMath::Median(pads.size(),pads.data());
  return out;
}
float getRMSCalDet(CalDet<float>& calDet) {
  float out;
  std::vector<float> pads;
  for (auto& calArray : calDet.getData()) {
    std::vector<float> data = calArray.getData();
    pads.insert( pads.end(), data.begin(), data.end() );
  }
  out = TMath::RMS(pads.size(),pads.data());
  return out;
}

void getMeanCalDets(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects)
{
  //CalDet<float> out;
  out = (float)0.;
  for(auto& calDet : mCalDetObjects) {
    out += calDet;
  }
  out /= static_cast<float>(mCalDetObjects.size());
  return;
}

void getMedianCalDets(CalDet<float>& out,const std::vector<CalDet<float>>& mCalDetObjects)
{
  out = (float)0.;
  uint16_t rocNumber = 0;
  int ntime = mCalDetObjects.size();
  float padVal[ntime];
  
  auto &outData = out.getData();
  for (size_t i = 0; i < outData.size(); i++) {
    auto &vals = outData[i].getData();
    std::vector<std::vector<float>> invals;
    for(int itime = 0; itime < ntime; itime++) {
      auto inData = mCalDetObjects[itime].getData();
      invals.push_back(inData[i].getData());
    }
    for (int j=0; j< (int)vals.size();j++) {
      for(int itime = 0; itime < ntime; itime++) {
        padVal[itime] = invals[itime][j];
      }
      vals[j] = TMath::Median(ntime,padVal);
    }
  }
  return;
}

void getRMSCalDets(CalDet<float>& out,const std::vector<CalDet<float>>& mCalDetObjects)
{
  out = (float)0.;
  uint16_t rocNumber = 0;
  int ntime = mCalDetObjects.size();
  float padVal[ntime];
  
  auto &outData = out.getData();
  for (size_t i = 0; i < outData.size(); i++) {
    auto &vals = outData[i].getData();
    std::vector<std::vector<float>> invals;
    for(int itime = 0; itime < ntime; itime++) {
      auto inData = mCalDetObjects[itime].getData();
      invals.push_back(inData[i].getData());
    }
    for (int j=0; j< (int)vals.size();j++) {
      for(int itime = 0; itime < ntime; itime++) {
        padVal[itime] = invals[itime][j];
      }
      vals[j] = TMath::RMS(ntime,padVal);
    }
  }
  return;
}

void normalizeCalDets(std::vector<CalDet<float>>& mCalDetObjects)
{
  std::vector<float> pads;
  for(auto& calDet : mCalDetObjects) {
    float sum = 0;
    auto &calDetdata = calDet.getData();
    for (auto &calArray : calDetdata) {
      auto &data = calArray.getData();
      for (auto&& pad : data){
        sum += pad;
      }
    }
    if(sum != 0){
      for (auto &calArray : calDetdata) {
        auto &data = calArray.getData();
        for (auto&& pad : data){
          pad /= sum;
        }
      }
    }
  }
}

void normalizeCalDet(CalDet<float>& calDet) {
  std::vector<float> pads;
  float sum = 0;
  auto &calDetdata = calDet.getData();
  for (auto &calArray : calDetdata) {
    auto &data = calArray.getData();
    for (auto&& pad : data){
      sum += pad;
    }
  }
  if(sum != 0){
    for (auto &calArray : calDetdata) {
      auto &data = calArray.getData();
      for (auto&& pad : data){
        pad /= sum;
      }
    }
  }
}

std::vector<float> getTimeSeriesMean(const std::vector<CalDet<float>>& mCalDetObjects) {
  std::vector<float> out;
  std::vector<float> pads;
  for(auto& calDet : mCalDetObjects) {
    for (auto& calArray : calDet.getData()) {
      std::vector<float> data = calArray.getData();
      pads.insert( pads.end(), data.begin(), data.end() );
    }
    out.push_back(TMath::Mean(pads.begin(),pads.end()));
  }
  return out;
}

std::vector<float> getTimeSeriesMedian(const std::vector<CalDet<float>>& mCalDetObjects) {
  std::vector<float> out;
  std::vector<float> pads;
  for(auto& calDet : mCalDetObjects) {
    for (auto& calArray : calDet.getData()) {
      std::vector<float> data = calArray.getData();
      pads.insert( pads.end(), data.begin(), data.end() );
    }
    out.push_back(TMath::Median(pads.size(),pads.data()));
  }
  return out;
}

std::vector<float> getTimeSeriesRMS(const std::vector<CalDet<float>>& mCalDetObjects) {
  std::vector<float> out;
  std::vector<float> pads;
  for(auto& calDet : mCalDetObjects) {
    for (auto& calArray : calDet.getData()) {
      std::vector<float> data = calArray.getData();
      pads.insert( pads.end(), data.begin(), data.end() );
    }
    out.push_back(TMath::RMS(pads.size(),pads.data()));
  }
  return out;
}


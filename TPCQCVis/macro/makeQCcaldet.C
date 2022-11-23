#include <cmath>
#include <fmt/format.h>
#include <string_view>
#include <string>
#include <vector>
#include <filesystem>
#include <map>
#include <TMath.h>
#include <TGraph.h>

#include "Framework/Logger.h"
#include "CCDB/CcdbApi.h"
#include "TPCBase/CDBInterface.h"
#include "TPCBase/Utils.h"
#include "TPCBase/Mapper.h"
#include "TPCCalibration/CalibTreeDump.h"
#include "QualityControl/TPC/ClustersData.h"
#include "QualityControl/TPC/DCSPTemperature.h"

enum CalPadType {
  Single = 0,
  Map = 1,
  QCClusters = 2,
};

struct ObjInfo {
  std::string cdb;
  std::string path;
  CalPadType type;
};

struct AddFiles {
  std::string fileName;
  std::string calDetNames;
};

using AddFilesVec = std::vector<AddFiles>;
using ObjInfoVec = std::vector<ObjInfo>;

const ObjInfoVec defaultEntries{
        //{"http://alice-ccdb.cern.ch", "TPC/Calib/PadGainFull", CalPadType::Single},
        //{"http://alice-ccdb.cern.ch", "TPC/Calib/PedestalNoise", CalPadType::Map},
        //{"http://alice-ccdb.cern.ch", "TPC/Calib/Pulser", CalPadType::Map},
        {"http://ali-qcdb-gpn.cern.ch:8083", "qc/TPC/MO/Clusters/ClusterData", CalPadType::QCClusters},
        {"http://ali-qcdb-gpn.cern.ch:8083", "qc/TPC/MO/RawDigits/RawDigitData", CalPadType::QCClusters},
};

const AddFilesVec defaultFiles{
        {"ITParams.root", "fraction,expLambda"},
};

using namespace o2::tpc;
using QCCL = o2::quality_control_modules::tpc::ClustersData;

void addObjects(CalibTreeDump& dump, const ObjInfoVec& objs, long sor, long eor, TFile* file);
void addObjects(CalibTreeDump& dump, const AddFilesVec& objs);

std::vector<long> getTimestamps(o2::ccdb::CcdbApi mCDB, const ObjInfo obj, const long sor, const long eor);

void getMeanCalDet(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects);
void getMedianCalDet(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects);
void getRMSCalDet(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects);

void normalizeCalDets(std::vector<CalDet<float>>& mCalDetObjects);

std::vector<float> getMeans(const std::vector<CalDet<float>>& mCalDetObjects);
std::vector<float> getMedians(const std::vector<CalDet<float>>& mCalDetObjects);
std::vector<float> getRMSs(const std::vector<CalDet<float>>& mCalDetObjects);

o2::ccdb::CcdbApi mCDB;

void makeQCcaldet(int run, const AddFilesVec addFiles = defaultFiles, const ObjInfoVec objInfo = defaultEntries)
{
  std::string filename = "CalDet_" + std::to_string(run) + ".root";
  TFile* file(TFile::Open(filename.c_str(), "recreate"));
  o2::ccdb::CcdbApi c;
  c.init("http://alice-ccdb.cern.ch");
  std::map<std::string, std::string> headers, metadataRCT, metadata, mm;

  headers = c.retrieveHeaders(fmt::format("RCT/Info/RunInformation/{}", run), metadataRCT, -1);
  for(auto& key : headers) {
    std::cout << key.first << "\t" << key.second << "\n";
  }
  const auto sor = std::stol(headers["SOR"].data());
  auto eor = std::stol(headers["EOR"].data());

  headers = c.retrieveHeaders(fmt::format("GLO/Config/GRPECS/{}", sor), mm, -1);
  const auto eor2 = std::stol(headers["Valid-Until"].data());
  const auto sor2 = std::stol(headers["Valid-From"].data());

  if (eor == 0) {
    eor = eor2;
  }

  CalibTreeDump dump;
  addObjects(dump, objInfo, sor, eor, file);
  addObjects(dump, addFiles);

  dump.setAddFEEInfo();
  dump.dumpToFile(fmt::format("TreeDump_run_{}.root", run));

  file->Close();
}

void addObjects(CalibTreeDump& dump, const ObjInfoVec& objs, long sor, long eor, TFile* file)
{
  long time = (sor + eor)/2;
  std::map<std::string, std::string> metadata;

  for (const auto& obj : objs) {
    if (mCDB.getURL() != obj.cdb) {
      mCDB.init(obj.cdb);
    }
    if (obj.type == CalPadType::Single) {
      auto calDet = mCDB.retrieveFromTFileAny<CalDet<float>>(obj.path, metadata, time);
      dump.add(new CalDet<float>(*calDet));
      LOGP(info, "Added {}", calDet->getName());
    } else if (obj.type == CalPadType::Map) {
      auto calDetMap = mCDB.retrieveFromTFileAny<CDBInterface::CalPadMapType>(obj.path, metadata, time);
      for (auto& [name, calDet] : *calDetMap) {
        dump.add(new CalDet<float>(calDet));
        LOGP(info, "Added {}", calDet.getName());
      }
    } else if (obj.type == CalPadType::QCClusters) {
      // Get all objects from run
      auto times = getTimestamps(mCDB, obj, sor, eor);
      std::vector<CalDet<float>> digitNClusVec,digitQMaxVec;
      std::vector<CalDet<float>> clusNClusVec,clusQMaxVec,clusQTotVec,clusSigmaTimeVec,clusSigmaPadVec,clusTimeBinVec;
      for (auto& time : times) {
        auto qccl = mCDB.retrieveFromTFileAny<QCCL>(obj.path, metadata, time);
        auto& clInfo = qccl->getClusters();
        if (obj.path.find("RawDigit") != std::string::npos) {
          digitNClusVec.push_back(clInfo.getNClusters());
          digitQMaxVec.push_back(clInfo.getQMax());          
        } 
        else {
          clusNClusVec.push_back(clInfo.getNClusters());
          clusQTotVec.push_back(clInfo.getQMax());
          clusQMaxVec.push_back(clInfo.getQTot());
          clusSigmaTimeVec.push_back(clInfo.getSigmaTime());
          clusSigmaPadVec.push_back(clInfo.getSigmaPad());
          clusTimeBinVec.push_back(clInfo.getTimeBin());
        }
      }

      // normalize obtained CalDets
      normalizeCalDets(digitNClusVec);
      normalizeCalDets(digitQMaxVec);
      normalizeCalDets(clusNClusVec);
      normalizeCalDets(clusQTotVec);
      normalizeCalDets(clusQMaxVec);
      normalizeCalDets(clusSigmaTimeVec);
      normalizeCalDets(clusSigmaPadVec);
      normalizeCalDets(clusTimeBinVec);
      
      Float_t timesArr [times.size()];
      transform(times.begin(), times.end(), timesArr,[](const long & elem){return (float)elem;});

      // Means
      auto gDigitNClusMeans = new TGraph((int)times.size(),timesArr,getMeans(digitNClusVec).data());
      auto gDigitQMaxVecMeans = new TGraph((int)times.size(),timesArr,getMeans(digitQMaxVec).data());
      auto gClusNClusVecMeans = new TGraph((int)times.size(),timesArr,getMeans(clusNClusVec).data());
      auto gClusQTotVecMeans = new TGraph((int)times.size(),timesArr,getMeans(clusQTotVec).data());
      auto gClusQMaxVecMeans = new TGraph((int)times.size(),timesArr,getMeans(clusQMaxVec).data());
      auto gClusSigmaTimeVecMeans = new TGraph((int)times.size(),timesArr,getMeans(clusSigmaTimeVec).data());
      auto gClusSigmaPadVecMeans = new TGraph((int)times.size(),timesArr,getMeans(clusSigmaPadVec).data());
      auto gClusTimeBinVecMeans = new TGraph((int)times.size(),timesArr,getMeans(clusTimeBinVec).data());    
      
      file->WriteObject(gDigitNClusMeans,"gDigitNClusMeans");
      file->WriteObject(gDigitQMaxVecMeans,"gDigitQMaxVecMeans");
      file->WriteObject(gClusNClusVecMeans,"gClusNClusVecMeans");
      file->WriteObject(gClusQTotVecMeans,"gClusQTotVecMeans");
      file->WriteObject(gClusQMaxVecMeans,"gClusQMaxVecMeans");
      file->WriteObject(gClusSigmaTimeVecMeans,"gClusSigmaTimeVecMeans");
      file->WriteObject(gClusSigmaPadVecMeans,"gClusSigmaPadVecMeans");
      file->WriteObject(gClusTimeBinVecMeans,"gClusTimeBinVecMeans");

      // Medians
      auto gDigitNClusMedians = new TGraph((int)times.size(),timesArr,getMedians(digitNClusVec).data());
      auto gDigitQMaxVecMedians = new TGraph((int)times.size(),timesArr,getMedians(digitQMaxVec).data());
      auto gClusNClusVecMedians = new TGraph((int)times.size(),timesArr,getMedians(clusNClusVec).data());
      auto gClusQTotVecMedians = new TGraph((int)times.size(),timesArr,getMedians(clusQTotVec).data());
      auto gClusQMaxVecMedians = new TGraph((int)times.size(),timesArr,getMedians(clusQMaxVec).data());
      auto gClusSigmaTimeVecMedians = new TGraph((int)times.size(),timesArr,getMedians(clusSigmaTimeVec).data());
      auto gClusSigmaPadVecMedians = new TGraph((int)times.size(),timesArr,getMedians(clusSigmaPadVec).data());
      auto gClusTimeBinVecMedians = new TGraph((int)times.size(),timesArr,getMedians(clusTimeBinVec).data());    
      
      file->WriteObject(gDigitNClusMedians,"gDigitNClusMedians");
      file->WriteObject(gDigitQMaxVecMedians,"gDigitQMaxVecMedians");
      file->WriteObject(gClusNClusVecMedians,"gClusNClusVecMedians");
      file->WriteObject(gClusQTotVecMedians,"gClusQTotVecMedians");
      file->WriteObject(gClusQMaxVecMedians,"gClusQMaxVecMedians");
      file->WriteObject(gClusSigmaTimeVecMedians,"gClusSigmaTimeVecMedians");
      file->WriteObject(gClusSigmaPadVecMedians,"gClusSigmaPadVecMedians");
      file->WriteObject(gClusTimeBinVecMedians,"gClusTimeBinVecMedians");

      // RMSs
      auto gDigitNClusRMSs = new TGraph((int)times.size(),timesArr,getRMSs(digitNClusVec).data());
      auto gDigitQMaxVecRMSs = new TGraph((int)times.size(),timesArr,getRMSs(digitQMaxVec).data());
      auto gClusNClusVecRMSs = new TGraph((int)times.size(),timesArr,getRMSs(clusNClusVec).data());
      auto gClusQTotVecRMSs = new TGraph((int)times.size(),timesArr,getRMSs(clusQTotVec).data());
      auto gClusQMaxVecRMSs = new TGraph((int)times.size(),timesArr,getRMSs(clusQMaxVec).data());
      auto gClusSigmaTimeVecRMSs = new TGraph((int)times.size(),timesArr,getRMSs(clusSigmaTimeVec).data());
      auto gClusSigmaPadVecRMSs = new TGraph((int)times.size(),timesArr,getRMSs(clusSigmaPadVec).data());
      auto gClusTimeBinVecRMSs = new TGraph((int)times.size(),timesArr,getRMSs(clusTimeBinVec).data());    
      
      file->WriteObject(gDigitNClusRMSs,"gDigitNClusRMSs");
      file->WriteObject(gDigitQMaxVecRMSs,"gDigitQMaxVecRMSs");
      file->WriteObject(gClusNClusVecRMSs,"gClusNClusVecRMSs");
      file->WriteObject(gClusQTotVecRMSs,"gClusQTotVecRMSs");
      file->WriteObject(gClusQMaxVecRMSs,"gClusQMaxVecRMSs");
      file->WriteObject(gClusSigmaTimeVecRMSs,"gClusSigmaTimeVecRMSs");
      file->WriteObject(gClusSigmaPadVecRMSs,"gClusSigmaPadVecRMSs");
      file->WriteObject(gClusTimeBinVecRMSs,"gClusTimeBinVecRMSs");

      if (obj.path.find("RawDigit") != std::string::npos) {
        CalDet<float> nDigitNClusMean, nDigitNClusMedian, nDigitNClusRMS;
        getMeanCalDet(nDigitNClusMedian,digitNClusVec);
        getMedianCalDet(nDigitNClusMean,digitNClusVec);
        getRMSCalDet(nDigitNClusRMS,digitNClusVec);
        nDigitNClusMean.setName("N_Digits_NClus_Mean");
        nDigitNClusMedian.setName("N_Digits_NClus_Median");
        nDigitNClusRMS.setName("N_Digits_NClus_RMS");
        file->WriteObject(&nDigitNClusMean, nDigitNClusMean.getName().c_str());
        file->WriteObject(&nDigitNClusMedian, nDigitNClusMedian.getName().c_str());
        file->WriteObject(&nDigitNClusRMS, nDigitNClusRMS.getName().c_str());
        LOGP(info, "Added {}", nDigitNClusMean.getName());
        LOGP(info, "Added {}", nDigitNClusMedian.getName());
        LOGP(info, "Added {}", nDigitNClusRMS.getName());

        CalDet<float> nDigitQMaxMean;
        CalDet<float> nDigitQMaxMedian;
        CalDet<float> nDigitQMaxRMS;
        getMeanCalDet(nDigitQMaxMean,digitQMaxVec);
        getMedianCalDet(nDigitQMaxMedian,digitQMaxVec);
        getRMSCalDet(nDigitQMaxRMS,digitQMaxVec);
        nDigitQMaxMean.setName("N_Digits_QMax_Mean");
        nDigitQMaxMedian.setName("N_Digits_QMax_Median");
        nDigitQMaxRMS.setName("N_Digits_QMax_RMS");
        file->WriteObject(&nDigitQMaxMean, nDigitQMaxMean.getName().c_str());
        file->WriteObject(&nDigitQMaxMedian, nDigitQMaxMedian.getName().c_str());
        file->WriteObject(&nDigitQMaxRMS, nDigitQMaxRMS.getName().c_str());
        LOGP(info, "Added {}", nDigitQMaxMean.getName());
        LOGP(info, "Added {}", nDigitQMaxMedian.getName());
        LOGP(info, "Added {}", nDigitQMaxRMS.getName()); 
      }
      else {
        CalDet<float> nclusNClusMean, nclusNClusMedian, nclusNClusRMS;
        getMeanCalDet(nclusNClusMean,clusNClusVec);
        getMedianCalDet(nclusNClusMedian,clusNClusVec);
        getRMSCalDet(nclusNClusRMS,clusNClusVec);
        nclusNClusMean.setName("N_clus_NClus_Mean");
        nclusNClusMedian.setName("N_clus_NClus_Median");
        nclusNClusRMS.setName("N_clus_NClus_RMS");
        file->WriteObject(&nclusNClusMean, nclusNClusMean.getName().c_str());
        file->WriteObject(&nclusNClusMedian, nclusNClusMedian.getName().c_str());
        file->WriteObject(&nclusNClusRMS, nclusNClusRMS.getName().c_str());
        LOGP(info, "Added {}", nclusNClusMean.getName());
        LOGP(info, "Added {}", nclusNClusMedian.getName());
        LOGP(info, "Added {}", nclusNClusRMS.getName());

        CalDet<float> nclusQMaxMean, nclusQMaxMedian, nclusQMaxRMS;
        getMeanCalDet(nclusQMaxMean,clusQMaxVec);
        getMedianCalDet(nclusQMaxMedian,clusQMaxVec);
        getRMSCalDet(nclusQMaxMean,clusQMaxVec);
        nclusQMaxMean.setName("N_clus_QMax_Mean");
        nclusQMaxMedian.setName("N_clus_QMax_Median");
        nclusQMaxRMS.setName("N_clus_QMax_RMS");
        file->WriteObject(&nclusQMaxMean, nclusQMaxMean.getName().c_str());
        file->WriteObject(&nclusQMaxMedian, nclusQMaxMedian.getName().c_str());
        file->WriteObject(&nclusQMaxRMS, nclusQMaxRMS.getName().c_str());
        LOGP(info, "Added {}", nclusQMaxMean.getName());
        LOGP(info, "Added {}", nclusQMaxMedian.getName());
        LOGP(info, "Added {}", nclusQMaxRMS.getName()); 

        CalDet<float> nclusQTotMean, nclusQTotMedian, nclusQTotRMS;
        getMeanCalDet(nclusQTotMean,clusQTotVec);
        getMedianCalDet(nclusQTotMedian,clusQTotVec);
        getRMSCalDet(nclusQTotRMS,clusQTotVec);
        nclusQTotMean.setName("N_clus_QTot_Mean");
        nclusQTotMedian.setName("N_clus_QTot_Median");
        nclusQTotRMS.setName("N_clus_QTot_RMS");
        file->WriteObject(&nclusQTotMean, nclusQTotMean.getName().c_str());
        file->WriteObject(&nclusQTotMedian, nclusQTotMedian.getName().c_str());
        file->WriteObject(&nclusQTotRMS, nclusQTotRMS.getName().c_str());
        LOGP(info, "Added {}", nclusQTotMean.getName());
        LOGP(info, "Added {}", nclusQTotMedian.getName());
        LOGP(info, "Added {}", nclusQTotRMS.getName());

        CalDet<float> nclusSigmaTimeMean, nclusSigmaTimeMedian, nclusSigmaTimeRMS;
        getMeanCalDet(nclusSigmaTimeMean,clusSigmaTimeVec);
        getMedianCalDet(nclusSigmaTimeMedian,clusSigmaTimeVec);
        getRMSCalDet(nclusSigmaTimeRMS,clusSigmaTimeVec);
        nclusSigmaTimeMean.setName("N_clus_SigmaTime_Mean");
        nclusSigmaTimeMedian.setName("N_clus_SigmaTime_Median");
        nclusSigmaTimeRMS.setName("N_clus_SigmaTime_RMS");
        file->WriteObject(&nclusSigmaTimeMean, nclusSigmaTimeMean.getName().c_str());
        file->WriteObject(&nclusSigmaTimeMedian, nclusSigmaTimeMedian.getName().c_str());
        file->WriteObject(&nclusSigmaTimeRMS, nclusSigmaTimeRMS.getName().c_str());
        LOGP(info, "Added {}", nclusSigmaTimeMean.getName());
        LOGP(info, "Added {}", nclusSigmaTimeMedian.getName());
        LOGP(info, "Added {}", nclusSigmaTimeRMS.getName()); 

        CalDet<float> nclusSigmaPadMean, nclusSigmaPadMedian, nclusSigmaPadRMS;
        getMeanCalDet(nclusSigmaPadMean,clusSigmaPadVec);
        getMedianCalDet(nclusSigmaPadMedian,clusSigmaPadVec);
        getRMSCalDet(nclusSigmaPadRMS,clusSigmaPadVec);
        nclusSigmaPadMean.setName("N_clus_SigmaPad_Mean");
        nclusSigmaPadMedian.setName("N_clus_SigmaPad_Median");
        nclusSigmaPadRMS.setName("N_clus_SigmaPad_RMS");
        file->WriteObject(&nclusSigmaPadMean, nclusSigmaPadMean.getName().c_str());
        file->WriteObject(&nclusSigmaPadMedian, nclusSigmaPadMedian.getName().c_str());
        file->WriteObject(&nclusSigmaPadRMS, nclusSigmaPadRMS.getName().c_str());
        LOGP(info, "Added {}", nclusSigmaPadMean.getName());
        LOGP(info, "Added {}", nclusSigmaPadMedian.getName());
        LOGP(info, "Added {}", nclusSigmaPadRMS.getName()); 

        CalDet<float> nclusTimeBinMean, nclusTimeBinMedian, nclusTimeBinRMS;
        getMeanCalDet(nclusTimeBinMean,clusTimeBinVec);
        getMedianCalDet(nclusTimeBinMedian,clusTimeBinVec);
        getRMSCalDet(nclusTimeBinRMS,clusTimeBinVec);
        nclusTimeBinMean.setName("N_clus_TimeBin_Mean");
        nclusTimeBinMedian.setName("N_clus_TimeBin_Median");
        nclusTimeBinRMS.setName("N_clus_TimeBin_RMS");
        file->WriteObject(&nclusTimeBinMean, nclusTimeBinMean.getName().c_str());
        file->WriteObject(&nclusTimeBinMedian, nclusTimeBinMedian.getName().c_str());
        file->WriteObject(&nclusTimeBinRMS, nclusTimeBinRMS.getName().c_str());
        LOGP(info, "Added {}", nclusTimeBinMean.getName());
        LOGP(info, "Added {}", nclusTimeBinMedian.getName());
        LOGP(info, "Added {}", nclusTimeBinRMS.getName());
      }
    }
  }
}

void addObjects(CalibTreeDump& dump, const AddFilesVec& objs)
{
  for (const auto& fileInfo : objs) {
    if (!std::filesystem::exists(fileInfo.fileName)) {
      LOGP(warning, "requested file '{}' does not exist", fileInfo.fileName);
      continue;
    }

    auto calPads = utils::readCalPads(fileInfo.fileName, fileInfo.calDetNames);
    for (auto calPad : calPads) {
      dump.add(calPad);
    }
  }
}

std::vector<long> getTimestamps(o2::ccdb::CcdbApi mCDB, const ObjInfo obj, const long sor, const long eor)
{
  std::vector<long> outVec;
  o2::quality_control_modules::tpc::DCSPTemperature DCSPtools;
  std::vector<std::string> fileList = DCSPtools.splitString(mCDB.list(obj.path), "\n");
  for (auto& file : fileList) {
    long timestamp = DCSPtools.getTimestamp(file);
    if (timestamp <= eor && timestamp >= sor) {
      outVec.emplace_back(timestamp);
    }
  }
  return outVec;
}

void getMeanCalDet(CalDet<float>& out, const std::vector<CalDet<float>>& mCalDetObjects)
{
  //CalDet<float> out;
  out = (float)0.;
  for(auto& calDet : mCalDetObjects) {
    out += calDet;
  }
  out /= static_cast<float>(mCalDetObjects.size());
  return;
}

void getMedianCalDet(CalDet<float>& out,const std::vector<CalDet<float>>& mCalDetObjects)
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

void getRMSCalDet(CalDet<float>& out,const std::vector<CalDet<float>>& mCalDetObjects)
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

std::vector<float> getMeans(const std::vector<CalDet<float>>& mCalDetObjects) {
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

std::vector<float> getMedians(const std::vector<CalDet<float>>& mCalDetObjects) {
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

std::vector<float> getRMSs(const std::vector<CalDet<float>>& mCalDetObjects) {
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

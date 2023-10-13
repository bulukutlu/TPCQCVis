/*
  make calibration tree using
  singularity container with QC has to be used

  alienv enter QualityControl/latest-o2

 .L $NOTES/JIRA/ATO-611/makePadCalibTree.C+

 */

#include <cmath>
#include <fmt/format.h>
#include <string_view>
#include <string>
#include <vector>
#include <filesystem>
#include <map>

#include "Framework/Logger.h"
#include "CCDB/CcdbApi.h"
#include "TPCBase/CDBInterface.h"
#include "TPCBase/Utils.h"
#include "TPCCalibration/CalibTreeDump.h"
#include "QualityControl/TPC/ClustersData.h"

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
        {"http://alice-ccdb.cern.ch", "TPC/Calib/PadGainFull", CalPadType::Single},
        {"http://alice-ccdb.cern.ch", "TPC/Calib/PedestalNoise", CalPadType::Map},
        {"http://alice-ccdb.cern.ch", "TPC/Calib/Pulser", CalPadType::Map},
        {"http://ali-qcdb-gpn.cern.ch:8083", "qc/TPC/MO/Clusters/ClusterData", CalPadType::QCClusters},
        {"http://ali-qcdb-gpn.cern.ch:8083", "qc/TPC/MO/RawDigits/RawDigitData", CalPadType::QCClusters},
};

const AddFilesVec defaultFiles{
        {"ITParams.root", "fraction,expLambda"},
};

using namespace o2::tpc;
using QCCL = o2::quality_control_modules::tpc::ClustersData;

void addObjects(CalibTreeDump& dump, const ObjInfoVec& objs, long time);
void addObjects(CalibTreeDump& dump, const AddFilesVec& objs);

o2::ccdb::CcdbApi mCDB;

void makePadCalibTree_original(int run, const AddFilesVec addFiles = defaultFiles, const ObjInfoVec objInfo = defaultEntries)
{
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

  auto refTime = (sor + eor) / 2;

  CalibTreeDump dump;
  addObjects(dump, objInfo, refTime);
  addObjects(dump, addFiles);

  dump.setAddFEEInfo();
  dump.dumpToFile(fmt::format("TreeDump_run_{}.root", run));
}

void addObjects(CalibTreeDump& dump, const ObjInfoVec& objs, long time)
{
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
      auto qccl = mCDB.retrieveFromTFileAny<QCCL>(obj.path, metadata, time);
      auto& clInfo = qccl->getClusters();
      if (obj.path.find("RawDigit") != std::string::npos) {
        // digit info
        auto nDigit = new CalDet<float>(clInfo.getNClusters());
        auto qMax = new CalDet<float>(clInfo.getQMax());
        nDigit->setName("N_Digits");
        qMax->setName("Q_Max_Digits");
        dump.add(nDigit);
        LOGP(info, "Added {}", nDigit->getName());
        dump.add(qMax);
        LOGP(info, "Added {}", qMax->getName());
      } else {
        // cluster info
        dump.add(new CalDet<float>(clInfo.getNClusters()));
        LOGP(info, "Added {}", clInfo.getNClusters().getName());
        dump.add(new CalDet<float>(clInfo.getQMax()));
        LOGP(info, "Added {}", clInfo.getQMax().getName());
        dump.add(new CalDet<float>(clInfo.getQTot()));
        LOGP(info, "Added {}", clInfo.getQTot().getName());
        dump.add(new CalDet<float>(clInfo.getSigmaTime()));
        LOGP(info, "Added {}", clInfo.getSigmaTime().getName());
        dump.add(new CalDet<float>(clInfo.getSigmaPad()));
        LOGP(info, "Added {}", clInfo.getSigmaPad().getName());
        dump.add(new CalDet<float>(clInfo.getTimeBin()));
        LOGP(info, "Added {}", clInfo.getTimeBin().getName());
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
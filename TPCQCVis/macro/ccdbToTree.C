#include <stdexcept>
#include <unordered_map>
#include <string_view>
#include <vector>
#include "rapidjson/document.h"

#include "TTree.h"
#include "TFile.h"
#include "CCDB/CcdbApi.h"

#include "GRPCalibration/GRPDCSDPsProcessor.h"
#include "CommonUtils/TreeStreamRedirector.h"
#include "TPCCalibration/CalibTreeDump.h"
#include "DataFormatsTPC/CalibdEdxCorrection.h"
#include "DataFormatsTPC/LtrCalibData.h"
#include "TPCBase/CalDet.h"
#include "DataFormatsTPC/DCS.h"

using namespace o2::tpc;

struct ObjInfo {
  long time{};
  std::string name{};
};

enum CalibTypes : uint32_t {
  Temperature = 1 << 0,
  HV = 1 << 1,
  Gas = 1 << 2,
  TimeGain = 1 << 3,
  Ltr = 1 << 4,
  Env = 1 << 5,
  NCalib = 1 << 6,
};

#pragma link C++ class std::unordered_map < std::string, std::vector < std::pair < uint64_t, double>>> + ;

template <typename T>
void addToTree(o2::utils::TreeStream& tree, std::string_view path, int run = -1);
// void addToTree(TTree& tree, std::string_view path);

o2::ccdb::CcdbApi mCdbApi;
long mStartCreation = 1;
long mEndCreation = -1;

/// dump entries configured in bitmask "typeMask" between "startCreation" and "endCreation"
/// entries are dumped into one tree each for each calibration type
/// if run > 0, startCreation and endCreation are replaced by sor - 5min and eor + 5 min of the run
void ccdbToTree(long startCreation = 1, long endCreation = -1, uint32_t typeMask = NCalib - 1, int run = -1)
{
  mCdbApi.init("https://alice-ccdb.cern.ch");
  if (!mCdbApi.isHostReachable()) {
    throw std::runtime_error("host not reachable");
  }

  const long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  if (run <= 0) {
    mStartCreation = startCreation;
    mEndCreation = (endCreation < 0) ? mEndCreation = now : endCreation;
  } else {
    std::map<std::string, std::string> headers, metadataRCT, mm;
    headers = mCdbApi.retrieveHeaders(fmt::format("RCT/Info/RunInformation/{}", run), metadataRCT, -1);
    const auto sor = std::stol(headers["SOR"].data());
    auto eor = std::stol(headers["EOR"].data());

    if (eor == 0) {
      headers = mCdbApi.retrieveHeaders(fmt::format("GLO/Config/GRPECS/{}", sor), mm, -1);
      const auto eor2 = std::stol(headers["Valid-Until"].data());
      const auto sor2 = std::stol(headers["Valid-From"].data());
      eor = eor2;
    }
    mStartCreation = sor - 5 * 60 * 1000;
    mEndCreation = eor + 5 * 60 * 1000;

    LOGP(info, "using time range {} - {} for run {}", mStartCreation, mEndCreation, run);
  }

  //
  std::string fileName = fmt::format("ccdbDump.{}-{}.{}.{:x}.root", mStartCreation, mEndCreation, now, typeMask);
  if (run > 0) {
    fileName = fmt::format("ccdbDump.run_{}.{}-{}.{}.{:x}.root", run, mStartCreation, mEndCreation, now, typeMask);
  }
  o2::utils::TreeStreamRedirector pcstream(fileName.data(), "recreate");
  // auto& t = pcstream << "calib";

  if (typeMask & Temperature) {
    addToTree<dcs::Temperature>(pcstream << "Temp", "TPC/Calib/Temperature", run);
  }
  if (typeMask & HV) {
    addToTree<dcs::HV>(pcstream << "HV", "TPC/Calib/HV", run);
  }
  if (typeMask & Gas) {
    addToTree<dcs::Gas>(pcstream << "Gas", "TPC/Calib/Gas", run);
  }
  if (typeMask & TimeGain) {
    addToTree<CalibdEdxCorrection>(pcstream << "TimeGain", "TPC/Calib/TimeGain", run);
  }
  if (typeMask & Ltr) {
    addToTree<LtrCalibData>(pcstream << "Ltr", "TPC/Calib/Ltr", run);
  }
  if (typeMask & Env) {
    addToTree<o2::grp::GRPEnvVariables>(pcstream << "Env", "GLO/Config/EnvVars", run);
  }

  pcstream.Close();
}

template <typename T>
void addToTree(o2::utils::TreeStream& tree, std::string_view path, int run)
// void addToTree(TTree& tree, std::string_view path)
{
  auto json = mCdbApi.list(path.data(), false, "application/json");

  rapidjson::Document doc;
  doc.Parse(json.data());

  if (!doc.IsObject() || !doc.HasMember("objects") || !doc["objects"].IsArray()) {
    throw std::runtime_error(fmt::format("could not parse object list for {}", path));
  }

  std::map<std::string, std::string> mm;
  const auto& entries = doc["objects"].GetArray();
  for (const auto& entry : entries) {
    const auto startValidity = entry["validFrom"].GetInt64();
    if ((startValidity < mStartCreation) || (startValidity >= mEndCreation)) {
      continue;
    }
    try {
      fmt::print("add object {}\n", startValidity);
      auto obj = mCdbApi.retrieveFromTFileAny<T>(path.data(), mm, startValidity);
      auto headers = mCdbApi.retrieveHeaders(path.data(), mm, startValidity);
      auto eorG = std::stol(headers["Valid-Until"].data());
      auto sorG = std::stol(headers["Valid-From"].data());
      auto runNumber = run;
      if (headers.find("runNumber") != headers.end()) {
        runNumber = std::stoi(headers["runNumber"].data());
      }

      tree << "Valid_From=" << sorG
           << "Valid_Until=" << eorG
           << "run=" << runNumber;

      if constexpr (std::is_same_v<T, o2::grp::GRPEnvVariables>) {
        for (auto& [key, value] : obj->mEnvVars) {
          tree << (const char*)fmt::format("{}=", key).data() << value;
        }
      } else {
        tree << "val=" << obj;
      }
      tree << "\n";
    } catch (const std::exception& e) {
      LOGP(warning, "Could not get object for Valid-From {} ({})", startValidity, e.what());
    }
  }
}

void addObj(CalDet<float>& obj, CalibTreeDump& dump, std::string_view addName)
{
  obj.setName(fmt::format("{}_{}", obj.getName(), addName));
  dump.add(&obj);
}

void makeCEtree(std::vector<ObjInfo> infos)
{
  o2::ccdb::CcdbApi c;
  c.init("http://alice-ccdb.cern.ch");

  CalibTreeDump dump;
  std::map<std::string, std::string> mm;

  for (const auto& info : infos) {
    auto a = c.retrieveFromTFileAny<std::unordered_map<std::string, CalDet<float>>>("TPC/Calib/CE", mm, info.time);
    addObj(a->at("T0"), dump, info.name);
    addObj(a->at("Qtot"), dump, info.name);
    addObj(a->at("Width"), dump, info.name);
  }

  dump.dumpToFile("ceLastStripScan.root");
}

void makeTreeLaserScan()
{
  std::vector<ObjInfo> objs{
    {1657007270069, "ref"},
    {1657008093369, "p50"},
    {1657008798007, "p100"},
    {1657009542902, "m50"},
    {1657010492782, "m100"},
  };
  makeCEtree(objs);
}

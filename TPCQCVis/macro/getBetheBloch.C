// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "CPOYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// @file   getBetheBloch.C
/// @author Christian Reckziegel, christian.reckziegel@cern.ch
///

#include <iostream>
#include <array>
#include <vector>
#include <string>
#include "TFile.h"
#include <cmath>
#include "Framework/Logger.h"
// O2 includes
#include "CCDB/CcdbApi.h"
#include "ReconstructionDataFormats/PID.h"
#include "Framework/DataTypes.h"
#include "DataFormatsTPC/BetheBlochAleph.h"
//#include "TPCPIDResponse.h"
//#include "Common/Core/PID/TPCPIDResponse.h"

//________________________________________________Defining Response object class_______________________________________________________________________________________
namespace o2::pid::tpc
{

/// \brief Class to handle the TPC PID response

class Response
{

 public:
  Response() = default;
  ~Response() = default;

  /// Setter and Getter for the private parameters
  void SetBetheBlochParams(const std::array<float, 5>& betheBlochParams) { mBetheBlochParams = betheBlochParams; }
  void SetResolutionParamsDefault(const std::array<float, 2>& resolutionParamsDefault) { mResolutionParamsDefault = resolutionParamsDefault; }
  void SetResolutionParams(const std::vector<double>& resolutionParams) { mResolutionParams = resolutionParams; }
  void SetMIP(const float mip) { mMIP = mip; }
  void SetChargeFactor(const float chargeFactor) { mChargeFactor = chargeFactor; }
  void SetMultiplicityNormalization(const float multNormalization) { mMultNormalization = multNormalization; }
  void SetNClNormalization(const float nclnorm) { nClNorm = nclnorm; }
  void SetUseDefaultResolutionParam(const bool useDefault) { mUseDefaultResolutionParam = useDefault; }
  void SetParameters(const Response* response)
  {
    mBetheBlochParams = response->GetBetheBlochParams();
    mResolutionParamsDefault = response->GetResolutionParamsDefault();
    mResolutionParams = response->GetResolutionParams();
    mMIP = response->GetMIP();
    nClNorm = response->GetNClNormalization();
    mChargeFactor = response->GetChargeFactor();
    mMultNormalization = response->GetMultiplicityNormalization();
    mUseDefaultResolutionParam = response->GetUseDefaultResolutionParam();
  }

  const std::array<float, 5> GetBetheBlochParams() const { return mBetheBlochParams; }
  const std::array<float, 2> GetResolutionParamsDefault() const { return mResolutionParamsDefault; }
  const std::vector<double> GetResolutionParams() const { return mResolutionParams; }
  float GetMIP() const { return mMIP; }
  float GetNClNormalization() const { return nClNorm; }
  float GetChargeFactor() const { return mChargeFactor; }
  float GetMultiplicityNormalization() const { return mMultNormalization; }
  bool GetUseDefaultResolutionParam() const { return mUseDefaultResolutionParam; }

  /// Gets the expected signal of the track
  template <typename TrackType>
  float GetExpectedSignal(const TrackType& track, const o2::track::PID::ID id) const;
  /// Gets the expected resolution of the track
  template <typename CollisionType, typename TrackType>
  float GetExpectedSigma(const CollisionType& collision, const TrackType& trk, const o2::track::PID::ID id) const;
  /// Gets the number of sigmas with respect the expected value
  template <typename CollisionType, typename TrackType>
  float GetNumberOfSigma(const CollisionType& collision, const TrackType& trk, const o2::track::PID::ID id) const;
  // Number of sigmas with respect to expected for MC, defining a tune-on-data signal value
  template <typename CollisionType, typename TrackType>
  float GetNumberOfSigmaMCTuned(const CollisionType& collision, const TrackType& trk, const o2::track::PID::ID id, float mcTunedTPCSignal) const;
  /// Gets the deviation to the expected signal
  template <typename TrackType>
  float GetSignalDelta(const TrackType& trk, const o2::track::PID::ID id) const;
  /// Gets relative dEdx resolution contribution due to relative pt resolution
  float GetRelativeResolutiondEdx(const float p, const float mass, const float charge, const float resol) const;

  void PrintAll() const;

 private:
  std::array<float, 5> mBetheBlochParams = {0.03209809958934784, 19.9768009185791, 2.5266601063857674e-16, 2.7212300300598145, 6.080920219421387};
  std::array<float, 2> mResolutionParamsDefault = {0.07, 0.0};
  std::vector<double> mResolutionParams = {5.43799e-7, 0.053044, 0.667584, 0.0142667, 0.00235175, 1.22482, 2.3501e-7, 0.031585};
  float mMIP = 50.f;
  float mChargeFactor = 2.299999952316284f;
  float mMultNormalization = 11000.;
  bool mUseDefaultResolutionParam = true;
  float nClNorm = 152.f;

  ClassDefNV(Response, 3);

}; // class Response

inline void Response::PrintAll() const
{
  LOGP(info, "==== TPC PID response parameters: ====");
  for (int i = 0; i < static_cast<int>(mBetheBlochParams.size()); i++) {
    LOGP(info, "BB param [{}] = {}", i, mBetheBlochParams[i]);
  }
  LOGP(info, "use default resolution parametrization = {}", mUseDefaultResolutionParam);
  if (mUseDefaultResolutionParam) {
    LOGP(info, "Default Resolution parametrization: ");
    for (int i = 0; i < static_cast<int>(mResolutionParamsDefault.size()); i++) {
      LOGP(info, "Resolution param [{}] = {}", i, mResolutionParamsDefault[i]);
    }
  } else {
    for (int i = 0; i < static_cast<int>(mResolutionParams.size()); i++) {
      LOGP(info, "Resolution param [{}] = {}", i, mResolutionParams[i]);
    }
  }
  LOGP(info, "mMIP = {}", mMIP);
  LOGP(info, "mChargeFactor = {}", mChargeFactor);
  LOGP(info, "mMultNormalization = {}", mMultNormalization);
  LOGP(info, "nClNorm = {}", nClNorm);
}

} // namespace o2::pid::tpc
//_____________________________________________________________________________________________________________________________________________________________________

using namespace o2::pid::tpc;

/**
 * @brief Get SOR and EOR.
 * 
 * This helper function get the Start Of Run and End Of Run numbers.
 * The first will be used as timestamp.
 *
 * @param ccdb The CcdbApi object associated to the run number.
 * @param runNumber The run number aimed.
 *
 *
 * @see getBetheBloch() [Get Bethe-Bloch fit parameters.]
 */
std::pair<long, long> getSORandEOR(o2::ccdb::CcdbApi& ccdb, int runNumber) {
    std::string rctPath = "RCT/Info/RunInformation";
    std::string runPath = rctPath + "/" + std::to_string(runNumber);
    std::map<std::string, std::string> metadata;
    auto headers = ccdb.retrieveHeaders(runPath, metadata, -1);

    long sor = headers.count("STF") > 0 ? std::stol(headers["STF"]) : std::stol(headers["SOR"]);
    long eor = headers.count("ETF") > 0 ? std::stol(headers["ETF"]) : std::stol(headers["EOR"]);

    return {sor, eor};
}

/**
 * @brief Get Response object associated to TPC data from specific run.
 *
 * @param ccdb The CcdbApi object associated to the run number.
 * @param path Path to the Response object to be retrieved.
 * @param timestamp Timestamp associated to the run.
 * @param metadata Data containing specific info about chosen run.
 *
 *
 * @see getBetheBloch() [Get Bethe-Bloch fit parameters.]
 */
Response* retrieveTPCResponse(o2::ccdb::CcdbApi& ccdb, const std::string& path,
                    const int64_t timestamp,
                    std::map<std::string, std::string>& metadata) {

    std::map<std::string, std::string> headers;
    
    headers = ccdb.retrieveHeaders(path, metadata, timestamp);

    auto* obj = ccdb.retrieveFromTFileAny<Response>(path, metadata, timestamp);

    if (!obj) {
        std::cerr << "Failed to retrieve TPC PID Response object" << std::endl;
        return nullptr;
    }

    return obj;
    
}

/**
 * @brief Get Bethe-Bloch parameters.
 * 
 * This core function Get Bethe-Bloch parameters and
 * store in the output file "bethe_bloch_params.txt".
 *
 * @param runNumber The run number aimed.
 *
 * @note this macro should be used by the downloadFromAlien.py python script only,
 * which will get the parameters from the output file and delete it afterward.
 * 
 * @see main() [Get input argument as run number as call getBetheBloch().]
 */
std::array<float, 5> getBetheBloch(int runNumber) {
    const std::string path = "Analysis/PID/TPC/Response";
    
    o2::ccdb::CcdbApi ccdb;
    ccdb.init("http://alice-ccdb.cern.ch");

    // Get timestamp associated to run number
    auto [sor, eor] = getSORandEOR(ccdb, runNumber);
    
    int timestamp = sor; // timestamp = Start Of Run

    std::map<std::string, std::string> metadata;

    Response* response = retrieveTPCResponse(ccdb, path, timestamp, metadata);
    if (!response) {
        std::cerr << "Failed to retrieve TPC PID Response" << std::endl;
        return {0.0, 0.0, 0.0, 0.0, 0.0}; // Return dummy values or handle error appropriately
    }

    std::array<float, 5> bbParams = response->GetBetheBlochParams();
    
    bool printParams = false;
    if (printParams) {

      response->PrintAll();

      std::cout << "Bethe-Bloch parameters for run " << runNumber << ":" << std::endl;
      for (int iPar = 0; iPar < 5; iPar++) {
          std::cout << "BB param [" << iPar << "] = " << bbParams[iPar] << std::endl;
      }
    }
    
    std::cout << "Bethe-Bloch parameters for run " << runNumber << ":" << std::endl;
    for (int iPar = 0; iPar < 5; iPar++) {
        std::cout << "BB param [" << iPar << "] = " << bbParams[iPar] << std::endl;
    }

    delete response;

    return bbParams;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <run_number>" << std::endl;
        return 1;
    }
    int runNumber = std::stoi(argv[1]);
    getBetheBloch(runNumber);
    return 0;
}
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
/// @file   runCCDBlocalAPI.C
/// @author Berkin Ulukutlu, berkin.ulukutlu@cern.ch
///

#if !defined(__CLING__) || defined(__ROOTCLING__)
// Root includes
#include "TH2F.h"
#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TPad.h"
// O2 includes
#include "CCDB/CcdbApi.h"
#include "TPCQC/CalPadWrapper.h"
#include "TPCQC/Clusters.h"
//#include "QualityControl/TPC/ClustersData.h"
//#include "TPCBase/Painter.h"
//#include "DataFormatsTPC/TrackTPC.h"
//#include "DataFormatsTPC/ClusterNaitve.h"
//#include "TPCQC/Tracks.h"
//#include "TPCQC/Helpers.h"
#endif

using namespace o2;

struct datafile{
    string path;
    long timestamp;
    string type;
};

void runCCDBDownloadTarget(const std::string target_directory, const std::string paths, const std::string timestamps, const std::string types){
    ccdb::CcdbApi api;
    map<std::string, std::string> metadata;
    api.init("http://ccdb-test.cern.ch:8080");

    // Fill the structures
    for(const auto& file:files_vector) {
      datafile current_file;
      current_file.path = getPath(file);
      current_file.timestamp = getTimeStamp(file);
      current_file.type = getType(file);
      std::string name_of_bo = getFileName(current_file.path);
      if (name_of_bo == input_file) {
        files.push_back(current_file);
      }
    }
    std::cout << "Checkpoint 4\n";
    std::string output_file = "userfile.root";
    // Create TFile and write objects
    TFile tf(output_file.c_str(),"recreate");
    for (const auto& file : files) {
        if (file.type == "TH1F"){
            auto th1f = api.retrieveFromTFileAny<TH1F>(file.path,metadata,file.timestamp);
            tf.WriteObject(th1f, file.path.c_str());        }
        else if (file.type == "TH2F"){
            auto th2f = api.retrieveFromTFileAny<TH2F>(file.path,metadata,file.timestamp);
            tf.WriteObject(th2f, file.path.c_str());
        }
        else if (file.type == "TCanvas") {
            auto tcanvas = api.retrieveFromTFileAny<TCanvas>(file.path,metadata,file.timestamp);
            tf.WriteObject(tcanvas, file.path.c_str());   
        }
        else if (file.type == "o2::tpc::qc::CalPadWrapper") {
            auto calpad = api.retrieveFromTFileAny<o2::tpc::qc::CalPadWrapper>(file.path,metadata,file.timestamp);
            tf.WriteObject(calpad, file.path.c_str());
        }
        else {
            printf("Object %s has unknown file type %s.\n Skipping.\n", file.path.c_str(), file.type.c_str());
        }    
    }
    tf.Close();
}
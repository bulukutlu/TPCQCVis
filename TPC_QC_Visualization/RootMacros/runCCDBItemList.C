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
/// @file   runCCDBItemList.C
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
// TPCQCVis includes
#include "helper.h"
//#include "QualityControl/TPC/ClustersData.h"
//#include "TPCBase/Painter.h"
//#include "DataFormatsTPC/TrackTPC.h"
//#include "DataFormatsTPC/ClusterNaitve.h"
//#include "TPCQC/Tracks.h"
//#include "TPCQC/Helpers.h"
#endif

using namespace o2;
//using namespace o2::quality_control::core;
using namespace std;

// Find files
vector<string> splitToVector(string str, string token){
    vector<string>result;
    while(str.size()){
        int index = str.find(token);
        if(index!=static_cast<int>(string::npos)){
            result.push_back(str.substr(0,index));
            str = str.substr(index+token.size());
            if(str.size()==0)result.push_back(str);
        }else{
            result.push_back(str);
            str = "";
        }
    }
    return result;
}

string getPath(string str){
    string result;
    string token = "Path: ";
    int start = str.find(token)+token.size();
    int end = str.find("\n",start);
    result = str.substr(start,end-start);
    return result;
}

long getTimeStamp(string str){
    string result_str;
    long result;
    string token = "Validity: ";
    int start = str.find(token)+token.size();
    int end = str.find(" -",start);
    result_str = str.substr(start,end-start);
    string::size_type sz;
    result = stol(result_str,&sz);
    return result;
}

string getType(string str){
    string result;
    string token = "ObjectType = ";
    int start = str.find(token)+token.size();
    int end = str.find("\n",start);
    result = str.substr(start,end-start);
    return result;
}

bool fileComparitor (std::string i,std::string j) { return (getPath(i)<getPath(j)); }

void runCCDBItemList(){
    //std::cout << "Checkpoint 1\n";
    ccdb::CcdbApi api;  // init
    map<string, string> metadata; // can be empty
    api.init("http://ccdb-test.cern.ch:8080");
    //api.init("http://ali-qcdb.cern.ch:8083"); //For the real QCDB access (doesn't work. Need to investigate Proxy Seetings)
    string path = "qc/TPC/MO/";
    string folder = "Tracks/.*";


    //std::cout << "Checkpoint 2\n";
    // Read a list of all files in directory
    string file_list = api.list(path+folder);
    vector<std::string> files_vector = splitToVector(file_list,"\n\n"); //split different files information into vector
    files_vector.pop_back();
    std::sort(files_vector.begin(),files_vector.end(),fileComparitor);
    //std::cout << "Checkpoint 3\n";


    vector<std::string> directoryTree;
    int file_count = 0;
    std::string current_file;
    for(const auto& file:files_vector) {
        if (file_count == 0) {
            current_file = getPath(file);
            directoryTree.push_back(current_file+"\nType: "+getType(file));
        }
        if (current_file == getPath(file)) {
            directoryTree.push_back("-->"+to_string(getTimeStamp(file)));
        }
        else {
            current_file = getPath(file);
            directoryTree.push_back(current_file+"\nType: "+getType(file));
            directoryTree.push_back("-->"+to_string(getTimeStamp(file)));
        }
        file_count++;
    }
    //std::cout << "Checkpoint 4\n";
    std::ofstream output_file("../../Data/UserFiles/directoryTree.txt");
    std::ostream_iterator<std::string> output_iterator(output_file, "\n");
    std::copy(directoryTree.begin(), directoryTree.end(), output_iterator);
}
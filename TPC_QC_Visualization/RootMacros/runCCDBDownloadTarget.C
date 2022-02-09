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
/// @file   runCCDBDownloadTarget.C
/// @author Berkin Ulukutlu, berkin.ulukutlu@cern.ch
///

#if !defined(__CLING__) || defined(__ROOTCLING__)
// Root includes
#include "TH1F.h"
#include "TH2F.h"
#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TPad.h"
#include "TGraph.h"
#include "TNtuple.h"
#include "TDirectory.h"
// O2 includes
#include "CCDB/CcdbApi.h"
#include "TPCQC/CalPadWrapper.h"
#include "TPCQC/Clusters.h"
#endif

using namespace o2;

#define REMOVE_SPACES(x) x.erase(std::remove(x.begin(), x.end(), ' '), x.end())

template<class C, typename T>
bool contains(C&& c, T e) { return find(begin(c), end(c), e) != end(c); };

void runCCDBDownloadTarget(const std::string output_file,const std::vector<int> targetFileID){
    // Initialize connection with the CCDB
    ccdb::CcdbApi api;
    map<std::string, std::string> metadata;
    api.init("http://ccdb-test.cern.ch:8080");

    // Get object list file (should be generated with the runCCDBItemList.C)
    std::ifstream myFile("../../Data/UserFiles/CCDB.csv");
    if(!myFile.is_open()) throw std::runtime_error("Could not find CCDB item list file");

    // Find objects specified from the user input order list (targetFileID)
    int line_count=0;
    std::string line;
    std::vector<std::vector<std::string>> values;
    while(std::getline(myFile, line)) {
        line_count++;
        std::string line_value;
        std::vector<std::string> line_values;
        std::stringstream ss(line);
        while(std::getline(ss, line_value, ',')) {
            REMOVE_SPACES(line_value);
            line_values.push_back(line_value);
        }
        if(line_count > 1) {
            if(contains(targetFileID,stoi(line_values[0]))) values.emplace_back(line_values);
        }
    }

    // Get set of unique tasks in order list to create subdirectories in the TFile
    std::vector<std::string> set_task;
    for(int i=0; i<(int)values.size(); i++) {
        bool is_in=false;
        for(int j=0; j<(int)set_task.size();j++){
            if(values[i][5] == set_task[j]) is_in = true;
        }
        if(!is_in) set_task.emplace_back(values[i][5]);
    }

    // Output file
    TFile *tf = new TFile(output_file.c_str(),"recreate");
    TDirectory *folders[(int)set_task.size()];
    for(int i=0; i<(int)set_task.size();i++) folders[i] = tf->mkdir(set_task[i].c_str());

    // Loop over files in order list
    std::string file_type, file_path, file_name, file_task;
    long file_timestamp;
    for(int i=0; i<(int)values.size(); i++) {
        file_type = values[i][4];
        file_task = values[i][5];
        file_name = values[i][2];
        file_path = values[i][1]+values[i][2];
        file_timestamp = stol(values[i][3]);
        auto dir = folders[(int)(find(set_task.begin(),set_task.end(),file_task)-set_task.begin())];
        //printf("Will download file:%s,%ld,%s\n",file_path.c_str(),file_timestamp,file_type.c_str());
        if (file_type == "TH1F"){
            auto th1f = api.retrieveFromTFileAny<TH1F>(file_path,metadata,file_timestamp);
            dir->WriteObject(th1f, file_name.c_str());
        }
        else if (file_type == "TH2F"){
            auto th2f = api.retrieveFromTFileAny<TH2F>(file_path,metadata,file_timestamp);
            //th2f->SetDirectory(file_task);
            dir->WriteObject(th2f, file_name.c_str());
        }
        else if (file_type == "TCanvas") {
            auto tcanvas = api.retrieveFromTFileAny<TCanvas>(file_path,metadata,file_timestamp);
            //tcanvas->SetDirectory(file_task);
            dir->WriteObject(tcanvas, file_name.c_str());
        }
        else if (file_type == "TTree") {
            auto ttree = api.retrieveFromTFileAny<TTree>(file_path,metadata,file_timestamp);
            //ttree->SetDirectory(file_task);
            dir->WriteObject(ttree, file_name.c_str());
        }
        else if (file_type == "TGraph") {
            auto tgraph = api.retrieveFromTFileAny<TGraph>(file_path,metadata,file_timestamp);
            dir->WriteObject(tgraph, file_name.c_str());
        }
        else if (file_type == "TNtuple") {
            auto tntuple = api.retrieveFromTFileAny<TNtuple>(file_path,metadata,file_timestamp);
            dir->WriteObject(tntuple, file_name.c_str());
        }
        else {
            printf("Object %s has unknown file type %s.\n Skipping.\n", file_path.c_str(), file_type.c_str());
        }
        
    }
    
    tf->Close();
}
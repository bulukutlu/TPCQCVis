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
/// @file   testQCDBConnection.C
/// @author Berkin Ulukutlu, berkin.ulukutlu@cern.ch
///

#if !defined(__CLING__) || defined(__ROOTCLING__)
#include "CCDB/CcdbApi.h" // O2 includes
#endif

using namespace o2;

void testQCDBConnection(const std::string ccdb_url = "10.161.69.62:8083/"){
    // Initialize CCDB API
    ccdb::CcdbApi api;
    map<std::string, std::string> metadata;
    api.init(ccdb_url);

    // Choose which directory to list
    std::string path = "qc/TPC/MO/PID/.*";

    // Read a list of all files in directory
    std::string file_list = api.list(path);
    std::cout << file_list << std::endl;
}
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
/// @file   mergeTHn.C
/// @author Berkin Ulukutlu, berkin.ulukutlu@cern.ch
///

#if !defined(__CLING__) || defined(__ROOTCLING__)
// Root includes
#include "TH2F.h"
#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TPad.h"
#include "TDirectory.h"
#include "THnSparse.h"
#include "THn.h"
// O2 includes
// TPCQCVis includes
#endif


void mergeTHn(const std::string input_path){
  // read THnSparse
  auto file = TFile::Open(input_path.c_str());
  TDirectoryFile *dirRun = (TDirectoryFile*) (file->FindObjectAny("ExpertVis"));
  THn *thn1 = (THn*)dirRun->Get("ExpertVis;1");
  THn *thn2 = (THn*)dirRun->Get("ExpertVis;2");
  thn1->Add(thn2);

  // Output file
  TFile *hfile = hfile = TFile::Open("myMergedNonSparseTHns.root","RECREATE");
  thn1->Write();
}
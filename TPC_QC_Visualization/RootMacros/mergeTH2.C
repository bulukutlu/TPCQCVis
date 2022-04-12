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
/// @file   mergeTH2.C
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
// O2 includes
// TPCQCVis includes
#endif


void mergeTH2(const std::string input_path){
  // read THnSparse
  auto file = TFile::Open(input_path.c_str());
  TDirectoryFile *dirRun = (TDirectoryFile*) (file->FindObjectAny("PID"));
  TH2F *th2f1 = (TH2F*)dirRun->Get("hdEdxVsTgl;79");
  TH2F *th2f2 = (TH2F*)dirRun->Get("hdEdxVsTgl;86");
  th2f1->Add(th2f2);

  // Output file
  TFile *hfile = hfile = TFile::Open("myMergedTH2s.root","RECREATE");
  th2f1->Write();
}
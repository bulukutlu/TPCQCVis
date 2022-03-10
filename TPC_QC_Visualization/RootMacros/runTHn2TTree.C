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
/// @file   runTHn2TTree.C
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


void runTHn2TTree(const std::string input_path){
  // read THnSparse
  auto file = TFile::Open(input_path.c_str());
  TDirectoryFile *dirRun = (TDirectoryFile*) (file->FindObjectAny("ExpertVis"));
  THnSparse *thn = (THnSparse*)dirRun->Get("ExpertVis;1");
  Int_t* coord;
  Float_t weight;

  // Get info on histogram structure
  Int_t dims = thn->GetNdimensions();
  coord = (Int_t*) malloc (dims);
  std::vector<Float_t> variables(dims,0);
  std::vector<std::string> names;
  for (int i=0; i<dims; i++) names.push_back(thn->GetAxis(i)->GetTitle());

  // Output file
  TFile *hfile = hfile = TFile::Open("myTreefromTHnSparse.root","RECREATE");

  // create the TTree and its branches
  TTree *tree = new TTree("T","TTee created from ExpertVis THnSparse");
  for (int i=0; i < dims; i++) tree->Branch(names[i].c_str(),&variables[i],(names[i]+"/F").c_str());
  tree->Branch("weight",&weight,"weight/F");

  // loop over all filled bins and fill the TTree
  for (int i=0; i < thn->GetNbins(); i++) {
    weight = (Float_t)thn->GetBinContent(i,coord);
    for (int j=0; j < dims; j++) variables[j] = (Float_t)thn->GetAxis(j)->GetBinCenter(coord[j]);
    tree->Fill();
  }
  tree->Write();
}
#include "CCDB/BasicCCDBManager.h"
#include "TPCCalibration/IDCContainer.h"
#include "TPCCalibration/IDCGroupHelperSector.h"
#include "TPCCalibration/IDCCCDBHelper.h"
#include "TPCBase/CDBInterface.h"

using namespace o2::tpc;

void load(const long timestamp = 1667333217472, float zmin = 0, float zmax = 3, const char* path = "http://alice-ccdb.cern.ch")
{
  using namespace o2::tpc;
  o2::ccdb::BasicCCDBManager mCCDBManager = o2::ccdb::BasicCCDBManager::instance();
  mCCDBManager.setURL(path);
  mCCDBManager.setTimestamp(timestamp);

  using dataT = unsigned char;
  IDCDelta<dataT>* mIDCDeltaA = mCCDBManager.get<o2::tpc::IDCDelta<dataT>>(CDBTypeMap.at(CDBType::CalIDCDeltaA));
  IDCZero* mIDCZeroA = mCCDBManager.get<o2::tpc::IDCZero>(CDBTypeMap.at(CDBType::CalIDC0A));
  IDCOne* mIDCOneA = mCCDBManager.get<o2::tpc::IDCOne>(CDBTypeMap.at(CDBType::CalIDC1A));
  std::unique_ptr<IDCGroupHelperSector> mHelperSectorA = std::make_unique<IDCGroupHelperSector>(IDCGroupHelperSector{*mCCDBManager.get<o2::tpc::ParameterIDCGroupCCDB>("TPC/Calib/IDC/GROUPINGPAR/A")});

  IDCDelta<dataT>* mIDCDeltaC = mCCDBManager.get<o2::tpc::IDCDelta<dataT>>(CDBTypeMap.at(CDBType::CalIDCDeltaC));
  IDCZero* mIDCZeroC = mCCDBManager.get<o2::tpc::IDCZero>(CDBTypeMap.at(CDBType::CalIDC0C));
  IDCOne* mIDCOneC = mCCDBManager.get<o2::tpc::IDCOne>(CDBTypeMap.at(CDBType::CalIDC1C));

  IDCCCDBHelper<dataT> helper;
  helper.setIDCDelta(mIDCDeltaA, Side::A);
  helper.setIDCZero(mIDCZeroA, Side::A);
  helper.setIDCOne(mIDCOneA, Side::A);
  helper.setGroupingParameter(mHelperSectorA.get(), Side::A);
  helper.setIDCDelta(mIDCDeltaC, Side::C);
  helper.setIDCZero(mIDCZeroC, Side::C);
  helper.setIDCOne(mIDCOneC, Side::C);
  helper.setGroupingParameter(mHelperSectorA.get(), Side::C);

  helper.createOutlierMap(); // create outlier map for the IDC0 which are currently set (this has to be performed if rejectOutlier=true)
  const bool rejectOutlier = true;
  float scalingValIDCA = helper.scaleIDC0(Side::A, rejectOutlier);
  float scalingValIDCC = helper.scaleIDC0(Side::C, rejectOutlier);

  std::cout<<"Scaling Value for IDC0 A side: "<<scalingValIDCA<<std::endl;
  std::cout<<"Scaling Value for IDC0 C side: "<<scalingValIDCC<<std::endl;

  helper.drawIDCZeroSide(o2::tpc::Side::A, Form("IDCZeroSideA_%li.pdf", timestamp), zmin, zmax);
  helper.drawIDCZeroSide(o2::tpc::Side::C, Form("IDCZeroSideC_%li.pdf", timestamp), zmin, zmax);

  CalDet<float> idc0Cal = helper.getIDCZeroCalDet();
  std::vector<CalDet<float>> idcDeltaCal = helper.getIDCDeltaCalDet();

  // std::cout<<"dumping"<<std::endl;
  // helper.dumpToTree(Form("tree_idc_%li.root", timestamp));
}
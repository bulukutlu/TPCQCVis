#!/bin/bash

GLOBAL_SHMSIZE=$(( 128 << 30 ))
NHBPERTF=256
HOSTMEMSIZE=$(( 5 << 30 ))
CALIB_INSPEC="A:TPC/RAWDATA"
ARGS_ALL="-b --session ${USER} --shm-segment-size $GLOBAL_SHMSIZE"

runTracks="o2-raw-tf-reader-workflow $ARGS_ALL --input-data /bigDisk/TPC/testData/ --severity warning --onlyDet TPC | \
           o2-tpc-raw-to-digits-workflow $ARGS_ALL --input-spec "$CALIB_INSPEC" | \
           o2-tpc-reco-workflow $ARGS_ALL --input-type digitizer --output-type tracks --disable-mc --no-tpc-zs-on-the-fly --configKeyValues "HBFUtils.nHBFPerTF=$NHBPERTF" | \
           o2-qc $ARGS_ALL --config json://home/tklemenz/AliSoftware/QualityControl/Modules/TPC/run/tpcQCTracks_direct.json | \
           o2-dpl-run $ARGS_ALL --run"

runClusters="o2-raw-tf-reader-workflow $ARGS_ALL --input-data /home/berki/alice/o2_rawtf_run00529077_tf00001022_epn035.tf --severity warning --onlyDet TPC | \
             o2-tpc-raw-to-digits-workflow $ARGS_ALL --input-spec "$CALIB_INSPEC" | \
             o2-tpc-reco-workflow $ARGS_ALL --input-type digitizer --output-type clusters --disable-mc --no-tpc-zs-on-the-fly --configKeyValues "HBFUtils.nHBFPerTF=$NHBPERTF" | \
             o2-qc $ARGS_ALL --config json://home/berki/alice/QualityControl/Modules/TPC/run/tpcQCClusters_direct.json | \
             o2-dpl-run $ARGS_ALL --run"

runDigits="o2-raw-tf-reader-workflow $ARGS_ALL --input-data /home/berki/alice/o2_rawtf_run00529077_tf00001022_epn035.tf --severity warning --onlyDet TPC | \
           o2-tpc-raw-to-digits-workflow $ARGS_ALL --input-spec "$CALIB_INSPEC" | \
           o2-qc $ARGS_ALL --config json://home/berki/alice/QualityControl/Modules/TPC/run/tpcQCRawDigits_direct.json | \
           o2-dpl-run $ARGS_ALL --run"

runFull="o2-raw-tf-reader-workflow $ARGS_ALL --input-data /bigDisk/TPC/testData/ --severity warning --onlyDet TPC | \
         o2-tpc-raw-to-digits-workflow $ARGS_ALL --input-spec "$CALIB_INSPEC" | \
         o2-tpc-reco-workflow $ARGS_ALL --input-type digitizer --output-type clusters,tracks,send-clusters-per-sector --disable-mc --no-tpc-zs-on-the-fly --configKeyValues "HBFUtils.nHBFPerTF=$NHBPERTF" | \
         o2-qc $ARGS_ALL --config json://home/tklemenz/AliSoftware/QualityControl/Modules/TPC/run/tpcQCTasks.json | \
         o2-dpl-run $ARGS_ALL --run"



eval $runDigits


######
#o2-dpl-output-proxy -b --session default --dataspec "A:TPC/CLUSTERNATIVE" --channel-config "name=readout-proxy,type=pair,method=bind,address=ipc:///tmp/cluster-shoveling,transport=zeromq,rateLogging=1"| o2-tpc-test-cluster-task-min-example --session default -b
#o2-tpc-file-reader --tpc-native-cluster-reader '--infile tpc-native-clusters.root' --input-type clusters --session default --channel-config "name=reader,type=pair,method=connect,address=ipc:///tmp/cluster-shoveling,transport=zeromq,rateLogging=1"
#!/usr/bin/env bash
set -e
set -x
set -u

# Check for the correct number of command-line arguments
if [ "$#" -ne 3 ]; then
  echo "Usage: $0 <period> <runNumber> <apass>"
  exit 1
fi

period="$1"
runNumber="$2"
apass="$3"

# Construct the directory path based on the provided arguments
data_dir="/alice/data/2023/${period}/${runNumber}/${apass}"

alien_find "$data_dir" 'o2_*/QC.root' > qc.list
sed -i -e 's/^/alien:\/\//' qc.list
split -d -l 25 qc.list qc_list_

for QC_LIST in qc_list_*
do
  o2-qc-file-merger --enable-alien --input-files-list "${QC_LIST}" --output-file "merged_${QC_LIST}.root" &
done

wait $(jobs -p)

o2-qc-file-merger --input-files merged_* --output-file QC_fullrun_${runNumber}.root


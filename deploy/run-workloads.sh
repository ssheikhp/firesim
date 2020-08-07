#!/usr/bin/env bash

# This is some sugar around:
# ./firesim -c <ini> {launchrunfarm && infrasetup && runworkload && terminaterunfarm}
# And thus will only work for workloads that do not need run other applications
# between firesim calls


# Defaults
withlaunch=0
terminate=1
jobnums=0

command -v jq >/dev/null 2>&1 || {
	echo "Command jq is required to use this script! Please install e.g. with 'sudo yum install jq'." >&2
	exit 1
}

function usage
{
    echo "usage: run-workload.sh <runtime.ini> <workload.json> [-n number of jobs] [-H | -h | --help] [--noterminate] [--withlaunch]"
    echo "   runtime.ini:   the firesim-relative path to the runtime confiuration you'd like to run"
    echo "                  e.g. rocket4_1x.ini"
    echo "   workload.json: the workload json you'd like to run"
    echo "                  e.g. spec2017-ref.json"
    echo "   --withlaunch:  (Optional) will spin up a runfarm based on the ini"
    echo "   --noterminate: (Optional) will not forcibly terminate runfarm instances after runworkload"
}

if [ $# -lt 2 -o "$1" == "--help" -o "$1" == "-h" -o "$1" == "-H" ]; then
    usage
    exit 3
fi

ini=$1
shift

json=$1
shift

if [ ! -f $ini ]; then
	echo "Could not find $ini" >&2
	usage
	exit 1
fi

if [ ! -f workloads/$json ]; then
	echo "Could not find workloads/$json" >&2
	usage
	exit 1
fi


jobnums=$(grep -E 'f1_[0-9]+xlarges' "$ini" | awk -F '=' '{s+=$2}END{print s}')
maxjobs=$(jq '.workloads | length' workloads/$json)

echo "Detected $jobnums f1 instances from $ini"
echo "Detected $maxjobs in workloads/$json"

while test $# -gt 0
do
   case "$1" in
        --withlaunch)
            withlaunch=1
            ;;
        --noterminate)
            terminate=0;
            ;;
	-n)
		jobnums=$((0 + $2))
		shift
		;;
        -h | -H | -help)
            usage
            exit
            ;;
        --*) echo "ERROR: bad option $1"
            usage
            exit 1
            ;;
        *) echo "ERROR: bad argument $1"
            usage
            exit 2
            ;;
    esac
    shift
done

if [ $maxjobs -le $jobnums ]; then 
	echo "Will launch all $maxjobs jobs at once"
	runs=0
else
	runs=$((($maxjobs + $jobnums - 1) / $jobnums))
	echo "Will launch $jobnums jobs over $runs runs"
fi

trap "exit" INT
set -e
set -o pipefail

if [ "$withlaunch" -ne "0" ]; then
    firesim -c $ini launchrunfarm -x "workload workloadname $json"
fi

if [ $runs -eq 0 ]; then
	firesim -c $ini infrasetup -x "workload workloadname $json"
	firesim -c $ini runworkload -x "workload workloadname $json"
else
	tmpjson=$(mktemp -p "$(dirname $(which firesim))/workloads")
	for r in $(seq 0 $(($runs - 1))); do
		firstjob=$(($r * $jobnums))
		lastjob=$(($firstjob + $jobnums))
		echo "Launch job $(($firstjob + 1) to job $lastjob..."
		jq ". | .workloads |= .[$firstjob:$lastjob]" workloads/$json > $tmpjson
		firesim -c $ini infrasetup -x "workload workloadname $(basename $tmpjson)"
		firesim -c $ini runworkload -x "workload workloadname $(basename $tmpjson)"
	done
	rm $tmpjson
fi

if [ "$terminate" -eq "1" ]; then
    firesim -c $ini terminaterunfarm --forceterminate -x "workload workloadname $json"
fi

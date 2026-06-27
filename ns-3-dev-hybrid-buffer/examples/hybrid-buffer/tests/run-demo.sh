#!/bin/bash

if [ $# -lt 1 ]
then
  echo "缺少输入参数: "
  exit 1
fi

NS3="$(pwd)/../../.."
CUR_DIR="$(pwd)"
OUTPUT_DIR="/home/yqliu/hb-logs"
PLOT_DIR="$CUR_DIR/analysis"

if [ ! -d "$OUTPUT_DIR" ]; then
    mkdir $OUTPUT_DIR
fi

# BM algorithms
BM_HW=0

N_CORES=60

cd $NS3

# echo help
echohelp()
{
    echo "help: echo some help info for this script."
    echo "runall: run all the test file."
    echo "clean: clean all the file in the output dir."
    echo "kill: kill all hybrid-buffer-test process."
}

# kill all the test file
killfunc()
{
    PROCESS=`ps -ef| grep "hybrid-buffer-test2"|grep -v grep| grep -v PPID| awk '{print $2}'`
    for i in $PROCESS
    do
    echo "Kill the process "hybrid-buffer-test" with PPID $i"
    kill -9 $i
    done
}

# clean all the File
cleanFile()
{
    rm -rf $OUTPUT_DIR/*.xml
    rm -rf $OUTPUT_DIR/*.csv
    rm -rf $OUTPUT_DIR/*.txt
    echo "clean all the output file."
}

# run all test cases under this directory
runall()
{
    SRC_PRE="test2-tc"
    for src in "$CUR_DIR"/*; do
        if [[ "$src" == *"$SRC_PRE"*".cc"  && ! "$src" == *"-copy"* ]]; then
			while [[ $(( $(ps aux | grep 'test2-tc.*optimized' | wc -l) )) -gt $N_CORES ]];do
				sleep 30;
				echo "waiting for cores..."
			done
            src_name_ext=$(basename $src)
            src_name="${src_name_ext%.*}"
            # run this program only when no same program is running
            if [[ $(ps aux | grep ${src_name_ext} | grep -v grep | wc -l) -eq 0 ]]; then
                output_file="${OUTPUT_DIR}/${src_name}.txt"
                (./ns3 run --cwd=$OUTPUT_DIR "${src_name}") > ${output_file} 2>&1 &
                echo "run ${src_name} test."
                sleep 1
            fi
        fi
    done
}

plotAll()
{
    cd $PLOT_DIR
    python3 hb-plot.py
}

if [ $1 == "help" ]
then
echohelp
fi

if [ $1 == "clean" ]
then
cleanFile
fi

if [ $1 == "kill" ]
then
killfunc
fi

if [ $1 == "runall" ]
then
runall
fi

if [ $1 == "plotall" ]
then
plotAll
fi


#!/bin/bash

if [ $# -lt 1 ]
then
  echo "缺少输入参数: "
  exit 1
fi

if [ $# == 3 ]
then
  testcase=$2
  algorithm=$3
fi


echo $algorithm

NS3="$(pwd)/../../.."
CUR_DIR="$(pwd)"
OUTPUT_DIR="/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data"
PLOT_DIR="$CUR_DIR/analysis"
TRAFFIC_GEN_DIR="/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/TrafficGen"

if [ ! -d "$OUTPUT_DIR" ]; then
    mkdir $OUTPUT_DIR
fi

# BM algorithms
BM_HW=0

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
    PROCESS=`ps -ef| grep "hybrid-buffer-test"|grep -v grep| grep -v PPID| awk '{print $2}'`
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


runTest8()
{
  # OUTPUT_DIR="/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data"
  OUTPUT_DIR_real="${OUTPUT_DIR}/${algorithm}/" #"/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data/pbs/"
  new_output_real="${OUTPUT_DIR_real}/${testcase:19:6}/"
  echo "Starting testcase $testcase at $(date)"

  if [ "$algorithm" = "pbs" ]; then
    # 流量速率
    for item in 900 800 700 600 500 400 300 200 100
    do
      echo "Starting testcase $testcase withflowrate-$item at $(date)"
      final_dir="${OUTPUT_DIR_real}/${testcase:19:6}/${item}/"
      output_file_real="${final_dir}/$testcase.txt"
      rm -rf $final_dir/*.csv
      (./ns3 run --cwd=$final_dir "$testcase --Deephir_threshold=1 --algorithm_name=pbs --flow_rate=$item" ) > ${output_file_real}
    done
  else
    # 流量速率
    for flow_rate in 900 800 700 600 500 400 300 200 100
    do
      for Deephir_threshold in 0.2 0.5 1.0 2.0 4.0
      do
        echo "Starting testcase $testcase with flowrate=$flow_rate Deephir_threshold=$Deephir_threshold at $(date)"
        final_dir="${OUTPUT_DIR_real}/${testcase:19:6}/${Deephir_threshold}M/${flow_rate}/"
        output_file_real="${final_dir}/$testcase.txt"
        rm -rf $final_dir/*.csv
        (./ns3 run --cwd=$final_dir "$testcase --Deephir_threshold=$Deephir_threshold --algorithm_name=BMS --flow_rate=$flow_rate" ) > ${output_file_real}
      done
    done
  fi
  echo "Finished testcase $testcase at $(date)"
  
  cd "/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/analysis"
  testcase="tc2-08"
  echo "---正在画" $testcase
  python ploting_sigle.py tc2-08 pbs
  echo "---"$testcase "画完了" 
}


runTest9()
{
  echo "testcase已自动更变为$testcase"
  #testcase="hybrid-buffer-test-tc2-09"
  echo "${algorithm}"

  # OUTPUT_DIR="/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data"
  echo "Starting testcase $testcase at $(date)"

  if [[ "$algorithm" = "pbs" || "$algorithm" = "both" ]]; then
    OUTPUT_DIR_real="${OUTPUT_DIR}/pbs"
    echo "Starting testcase $testcase with pbs at $(date)"
    final_dir="${OUTPUT_DIR_real}/${testcase:19:6}/"
    output_file_real="${final_dir}/$testcase.txt"
    rm -rf $final_dir/*.csv
    (./ns3 run --cwd=$final_dir "$testcase --Deephir_threshold=1 --algorithm_name=pbs ") > ${output_file_real} &
    sleep 1
  fi
  if [[ "$algorithm" = "BMS" || "$algorithm" = "both" ]]; then
    # 流量速率
    OUTPUT_DIR_real="${OUTPUT_DIR}/BMS"
    for Deephir_threshold in 0.2 0.5 1.0 2.0 
    do
      echo "Starting testcase $testcase with Deephir_threshold=$Deephir_threshold at $(date)"
      final_dir="${OUTPUT_DIR_real}/${testcase:19:6}/${Deephir_threshold}M/"
      output_file_real="${final_dir}/$testcase.txt"
      rm -rf $final_dir/*.csv
      (./ns3 run --cwd=$final_dir "$testcase --Deephir_threshold=$Deephir_threshold --algorithm_name=BMS ") > ${output_file_real} &
      sleep 1
    done
    for Deephir_threshold in 4.0
    do
      echo "Starting testcase $testcase with Deephir_threshold=$Deephir_threshold at $(date)"
      final_dir="${OUTPUT_DIR_real}/${testcase:19:6}/${Deephir_threshold}M/"
      output_file_real="${final_dir}/$testcase.txt"
      rm -rf $final_dir/*.csv
      (./ns3 run --cwd=$final_dir "$testcase --Deephir_threshold=$Deephir_threshold --algorithm_name=BMS ") > ${output_file_real}
    done
  fi
  
  echo "Finished testcase $testcase  at $(date)"
  
  cd "/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/analysis"
  echo "---正在画" $testcase
  python TruePlot.py
  echo "---"$testcase "画完了" 
}

run()
{
  # OUTPUT_DIR="/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/data"
  OUTPUT_DIR_real="${OUTPUT_DIR}/${algorithm}/"

  new_output_real="${OUTPUT_DIR_real}/${testcase:19:6}/"

  output_file_real="${new_output_real}/$testcase.txt"

  echo "Starting testcase $testcase at $(date)"

  rm -rf $new_output_real/*.csv

  (./ns3 run --cwd=$new_output_real "$testcase --algorithm_name=$algorithm") > ${output_file_real}

  
  echo "Finished testcase $testcase at $(date)"
}

plotAll()
{
    cd $PLOT_DIR
    python3 hb-plot.py
}


showrun()
{
  PROCESS=`ps -ef| grep "hybrid-buffer-test"|grep -v grep| grep -v PPID| awk '{print $8}'`
  for i in $PROCESS
  do
    if [[ $i != "python3" ]]
    then
      echo "Runnning Testcase: "$i
    fi
  done
}

TrafficGen_wk(){
  echo "-------------------------------------------------"
  echo "这里输入的参数跟traffic_gen.py是一样的 这是参数列表"
  echo "-c,--cdf,default ="WebSearch_distribution.txt""
  echo "-n,--nhost"
  echo "-l,--load,default = "0.3""
  echo "-b,--bandwidth,default = "10G""
  echo "-t,--time,default = "10""
  echo "-o,--output,default = "Generated/traffic.txt""
  echo "-n输入numspokes的一半"
  echo "-------------------------------------------------"

  cd $TRAFFIC_GEN_DIR
  python3 traffic_gen_wk.py *$@ 
}

TrafficGen_original(){
  echo "-------------------------------------------------"
  echo "这里输入的参数跟traffic_gen.py是一样的 这是参数列表"
  echo "-c,--cdf,default ="WebSearch_distribution.txt""
  echo "-n,--nhost"
  echo "-l,--load,default = "0.3""
  echo "-b,--bandwidth,default = "10G""
  echo "-t,--time,default = "10""
  echo "-o,--output,default = "Generated/traffic.txt""
  echo "-n输入numspokes的一半"
  echo "-------------------------------------------------"

  cd $TRAFFIC_GEN_DIR
  python3 traffic_gen.py *$@ 
}

###############################################################################################
#一键用两种算法（pbs和deephir）跑tc2-05到tc2-07，典型场景、多突发、多拥塞，同时画图
run_and_draw_basic(){
  echo '正在用两种算法（pbs和deephir）跑从tc2-05到tc2-07所有的测试用例，同时一键画所有的图'

  algorithm="BMS"
  prefix="hybrid-buffer-test-"
  echo "  #以下算法为：" $algorithm
    testcase="${prefix}tc2-05"
      echo "---正在跑" $testcase
        run
      echo "---"$testcase "跑完了" 
    testcase="${prefix}tc2-06"
      echo "---正在跑" $testcase
        run
      echo "---"$testcase "跑完了" 
    testcase="${prefix}tc2-07"
      echo "---正在跑" $testcase
        run
      echo "---"$testcase "跑完了" 
  echo "  #算法" $algorithm "已结束"
  

  
  algorithm="pbs"
  echo "  #以下算法为：" $algorithm
    testcase="${prefix}tc2-05"
      echo "---正在跑" $testcase
        run
      echo "---"$testcase "跑完了" 
    testcase="${prefix}tc2-06"
      echo "---正在跑" $testcase
        run
      echo "---"$testcase "跑完了" 
    testcase="${prefix}tc2-07"
      echo "---正在跑" $testcase
        run
      echo "---"$testcase "跑完了" 
  echo "  #算法" $algorithm "已结束"
  

  echo "*程序跑完了"
  echo "###############################################################################################"
  echo "*正在画图……"

    cd "/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/analysis"
      testcase="tc2-05"
        echo "---正在画" $testcase
          python ploting_sigle.py $testcase BMS
          python ploting_sigle.py $testcase pbs
          python ploting_merge.py $testcase
        echo "---"$testcase "画完了" 
      testcase="tc2-06"
        echo "---正在画" $testcase
          python ploting_sigle.py $testcase BMS
          python ploting_sigle.py $testcase pbs
          python ploting_merge.py $testcase
        echo "---"$testcase "画完了" 
      testcase="tc2-07"
        echo "---正在画" $testcase
          python ploting_sigle.py $testcase BMS
          python ploting_sigle.py $testcase pbs
          python ploting_merge.py $testcase
        echo "---"$testcase "画完了" 
  echo "*图画完了"
}

###############################################################################################
#一键用两种算法（pbs和deephir）跑从tc2-05到tc2-09所有的测试用例，同时一键画所有的图
run_and_draw_all(){
  echo '正在用两种算法（pbs和deephir）跑从tc2-05到tc2-09所有的测试用例，同时一键画所有的图 ???'$algorithm

  if [[ "$algorithm" = "BMS" || "$algorithm" = "both" ]]; then  #代码有问题吗？ 报错：./run-tests.sh: 第 277 行: [: 缺少 `]'
    algorithm="BMS"
    prefix="hybrid-buffer-test-"
    echo "  #以下算法为：" $algorithm
      testcase="${prefix}tc2-05"
        echo "---正在跑" $testcase
          run
        echo "---"$testcase "跑完了" 
      testcase="${prefix}tc2-06"
        echo "---正在跑" $testcase
          run
        echo "---"$testcase "跑完了" 
      testcase="${prefix}tc2-07"
        echo "---正在跑" $testcase
          run
        echo "---"$testcase "跑完了" 
      testcase="${prefix}tc2-08"
        echo "---正在跑" $testcase
          runTest8
        echo "---"$testcase "跑完了" 
      testcase="${prefix}tc2-09"
        echo "---正在跑" $testcase
          runTest9
        echo "---"$testcase "跑完了" 
    echo "  #算法" $algorithm "已结束"
  fi

  if [[ "$algorithm" = "pbs" || "$algorithm" = "both" ]]; then
    algorithm="pbs"
    prefix="hybrid-buffer-test-"
    echo "  #以下算法为：" $algorithm
      testcase="${prefix}tc2-05"
        echo "---正在跑" $testcase
          run
        echo "---"$testcase "跑完了" 
      testcase="${prefix}tc2-06"
        echo "---正在跑" $testcase
          run
        echo "---"$testcase "跑完了" 
      testcase="${prefix}tc2-07"
        echo "---正在跑" $testcase
          run
        echo "---"$testcase "跑完了" 
      testcase="${prefix}tc2-08"
        echo "---正在跑" $testcase
          runTest8
        echo "---"$testcase "跑完了" 
      testcase="${prefix}tc2-09"
        echo "---正在跑" $testcase
          runTest9
        echo "---"$testcase "跑完了" 
    echo "  #算法" $algorithm "已结束"
  fi

  echo "*程序跑完了"
  echo "###############################################################################################"
  echo "*正在画图……"

    cd "/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/analysis"
      testcase="tc2-05"
        echo "---正在画" $testcase
          python ploting_sigle.py $testcase BMS
          python ploting_sigle.py $testcase pbs
          python ploting_merge.py $testcase
        echo "---"$testcase "画完了" 
      testcase="tc2-06"
        echo "---正在画" $testcase
          python ploting_sigle.py $testcase BMS
          python ploting_sigle.py $testcase pbs
          python ploting_merge.py $testcase
        echo "---"$testcase "画完了" 
      testcase="tc2-07"
        echo "---正在画" $testcase
          python ploting_sigle.py $testcase BMS
          python ploting_sigle.py $testcase pbs
          python ploting_merge.py $testcase
        echo "---"$testcase "画完了" 
      testcase="tc2-08"
        echo "---正在画" $testcase
          python ploting_sigle.py tc2-08 pbs
        echo "---"$testcase "画完了" 
      testcase="tc2-09"
        echo "---正在画" $testcase
          python TruePlot.py
        echo "---"$testcase "画完了" 
  echo "*图画完了"
}

draw_all(){
  echo "###############################################################################################"
  echo "*正在画图……"
    cd "/home/dell6/yrf/pba-xzx/ns-3-dev-hybrid-buffer/examples/hybrid-buffer/tests/analysis"
      testcase="tc2-05"
        echo "---正在画" $testcase
          python ploting_sigle.py $testcase BMS
          python ploting_sigle.py $testcase pbs
          python ploting_merge.py $testcase
        echo "---"$testcase "画完了" 
      testcase="tc2-06"
        echo "---正在画" $testcase
          python ploting_sigle.py $testcase BMS
          python ploting_sigle.py $testcase pbs
          python ploting_merge.py $testcase
        echo "---"$testcase "画完了" 
      testcase="tc2-07"
        echo "---正在画" $testcase
          python ploting_sigle.py $testcase BMS
          python ploting_sigle.py $testcase pbs
          python ploting_merge.py $testcase
        echo "---"$testcase "画完了" 
      testcase="tc2-08"
        echo "---正在画" $testcase
          python ploting_sigle.py tc2-08 pbs
        echo "---"$testcase "画完了" 
      testcase="tc2-09"
        echo "---正在画" $testcase
          python TruePlot.py
        echo "---"$testcase "画完了" 
  echo "*图画完了"
  echo "###############################################################################################"
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

if [ $1 == "runTest8" ]
then
runTest8
fi

if [ $1 == "run" ]
then
run
fi

if [ $1 == "show" ]
then
showrun
fi

if [ $1 == "TrafficGenWK" ]
then
TrafficGen_wk
fi

if [ $1 == "TrafficGen" ]
then
TrafficGen_original
fi

if [ $1 == "runTest9" ]
then
runTest9
fi

if [ $1 == "run_and_draw_all" ]
then
run_and_draw_all
fi

if [ $1 == "run_and_draw_basic" ]
then
run_and_draw_basic
fi

if [ $1 == "auto_run_draw" ]
then
auto_run_draw
fi

if [ $1 == "draw_all" ]
then
draw_all
fi

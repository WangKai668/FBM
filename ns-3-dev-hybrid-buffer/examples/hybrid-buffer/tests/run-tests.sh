#!/bin/bash

# 添加一键运行命令
if [[ $# -lt 1 ]]; then
    echo "错误：缺少命令参数"
    echo
    echo "单场景运行："
    echo "  ./run-tests.sh run hybrid-buffer-test-tc2-05 pbs"
    echo
    echo "批量运行："
    echo "  ./run-tests.sh run_and_draw_all pbs"
    echo "  ./run-tests.sh run_and_draw_all BMS"
    echo "  ./run-tests.sh run_and_draw_all both"
    echo
    echo "基础场景："
    echo "  ./run-tests.sh run_and_draw_basic"
    echo
    echo "只画图："
    echo "  ./run-tests.sh draw_all"
    exit 1
fi

testcase=""
algorithm=""
case "$1" in
    run|runTest8|runTest9)
        if [[ $# -ne 3 ]]; then
            echo "示例：./run-tests.sh run hybrid-buffer-test-tc2-05 pbs"
            exit 1
        fi
        testcase="$2"
        algorithm="$3"

        if [[ "$1" == "runTest9" ]]; then
            if [[ "$algorithm" != "pbs" &&
                  "$algorithm" != "BMS" &&
                  "$algorithm" != "both" ]]; then
                echo "错误：算法只支持 pbs、BMS 或 both"
                exit 1
            fi
        else
            if [[ "$algorithm" != "pbs" &&
                  "$algorithm" != "BMS" ]]; then
                echo "错误：算法只支持 pbs 或 BMS"
                exit 1
            fi
        fi
        ;;
    run_and_draw_all)
        if [[ $# -ne 2 ]]; then
            echo "错误：run_and_draw_all 需要算法参数"
            echo "示例：./run-tests.sh run_and_draw_all both"
            exit 1
        fi
        algorithm="$2"

        if [[ "$algorithm" != "pbs" &&
              "$algorithm" != "BMS" &&
              "$algorithm" != "both" ]]; then
            echo "错误：算法只支持 pbs、BMS 或 both"
            exit 1
        fi
        ;;
esac


echo $algorithm

# run-tests.sh 文件自身所在的目录，即 tests 目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

CUR_DIR="$SCRIPT_DIR"

# 从 tests 目录向上三级，得到 ns-3-dev-hybrid-buffer 根目录
NS3="$(cd "$SCRIPT_DIR/../../.." && pwd)"

OUTPUT_DIR="$SCRIPT_DIR/data"
PLOT_DIR="$SCRIPT_DIR/analysis"
TRAFFIC_GEN_DIR="$SCRIPT_DIR/TrafficGen"

if [ ! -d "$OUTPUT_DIR" ]; then
    mkdir $OUTPUT_DIR
fi

# BM algorithms
BM_HW=0

cd $NS3

# 运行实验前只进行一次增量编译
case "$1" in
    run|runTest8|runTest9|run_and_draw_all|run_and_draw_basic)
        echo "开始检查并编译 ns-3..."
        ./ns3 build

        if [[ $? -ne 0 ]]; then
            echo "错误：ns-3 编译失败"
            exit 1
        fi

        echo "ns-3 编译完成，后续实验不再重复编译"
        ;;
esac

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


# runTest8()
# {
#   # 从 hybrid-buffer-test-tc2-08 中提取 tc2-08
#   local case_id="${testcase#hybrid-buffer-test-}"

#   local output_dir_real="${OUTPUT_DIR}/${algorithm}"
#   local final_dir
#   local output_file_real

#   echo "Starting testcase $testcase at $(date)"

#   if [[ "$algorithm" == "pbs" ]]; then

#     for flow_rate in 900 800 700 600 500 400 300 200 100
#     do
#       echo "Starting testcase $testcase with flow_rate=$flow_rate at $(date)"

#       final_dir="${output_dir_real}/${case_id}/${flow_rate}"
#       output_file_real="${final_dir}/${testcase}.txt"
#       mkdir -p "$final_dir"
#       rm -f "$final_dir"/*.csv

#       ./ns3 run --no-build --cwd="$final_dir" \
#         "$testcase --Deephir_threshold=1 --algorithm_name=pbs --flow_rate=$flow_rate" \
#         > "$output_file_real" 2>&1

#       if [[ $? -ne 0 ]]; then
#         echo "ERROR: $testcase pbs flow_rate=$flow_rate 运行失败"
#         echo "日志位置：$output_file_real"
#         return 1
#       fi

#       echo "Finished flow_rate=$flow_rate"
#     done

#   elif [[ "$algorithm" == "BMS" ]]; then

#     for flow_rate in 900 800 700 600 500 400 300 200 100
#     do
#       for Deephir_threshold in 0.2 0.5 1.0 2.0 4.0
#       do
#         echo "Starting testcase $testcase with flow_rate=$flow_rate Deephir_threshold=$Deephir_threshold at $(date)"

#         final_dir="${output_dir_real}/${case_id}/${Deephir_threshold}M/${flow_rate}"
#         output_file_real="${final_dir}/${testcase}.txt"

#         mkdir -p "$final_dir"

#         rm -f "$final_dir"/*.csv

#         ./ns3 run --no-build --cwd="$final_dir" \
#           "$testcase --Deephir_threshold=$Deephir_threshold --algorithm_name=BMS --flow_rate=$flow_rate" \
#           > "$output_file_real" 2>&1

#         if [[ $? -ne 0 ]]; then
#           echo "ERROR: $testcase BMS flow_rate=$flow_rate threshold=$Deephir_threshold 运行失败"
#           echo "日志位置：$output_file_real"
#           return 1
#         fi

#         echo "Finished flow_rate=$flow_rate threshold=$Deephir_threshold"
#       done
#     done

#   else
#     echo "ERROR: runTest8 只支持 pbs 或 BMS，当前参数为：$algorithm"
#     return 1
#   fi

#   echo "Finished testcase $testcase at $(date)"
# }


# 新的8号实验的运行代码
runTest8()
{
  # 从 hybrid-buffer-test-tc2-11 中提取 tc2-11
  local case_id="${testcase#hybrid-buffer-test-}"

  local output_dir_real="${OUTPUT_DIR}/${algorithm}"
  local final_dir
  local output_file_real

  # 新实验的自变量：
  # 500Gbps背景流持续时间，单位us
  local change_time_list=(5 10 15 30 45 60)

  echo "Starting testcase $testcase at $(date)"

  if [[ "$algorithm" == "pbs" ]]; then

    # FBM不需要遍历DeepHiR阈值
    for change_time_us in "${change_time_list[@]}"
    do
      echo "Starting testcase $testcase with change_time_us=${change_time_us}us algorithm=pbs at $(date)"

      final_dir="${output_dir_real}/${case_id}/${change_time_us}us"
      output_file_real="${final_dir}/${testcase}.txt"

      mkdir -p "$final_dir"

      # 删除上一次实验结果
      rm -f "$final_dir"/*.csv
      rm -f "$final_dir"/*.xml
      rm -f "$output_file_real"

      if ! ./ns3 run --no-build --cwd="$final_dir" \
        "$testcase \
        --Deephir_threshold=1 \
        --algorithm_name=pbs \
        --change_time_us=$change_time_us" \
        > "$output_file_real" 2>&1
      then
        echo "ERROR: $testcase pbs change_time_us=${change_time_us}us 运行失败"
        echo "日志位置：$output_file_real"
        return 1
      fi

      echo "Finished pbs change_time_us=${change_time_us}us"
    done

  elif [[ "$algorithm" == "BMS" ]]; then

    # DeepHiR继续遍历静态阈值
    for Deephir_threshold in 0.2 0.5 1.0 2.0 4.0
    do
      for change_time_us in "${change_time_list[@]}"
      do
        echo "Starting testcase $testcase with change_time_us=${change_time_us}us Deephir_threshold=${Deephir_threshold}M at $(date)"

        final_dir="${output_dir_real}/${case_id}/${Deephir_threshold}M/${change_time_us}us"
        output_file_real="${final_dir}/${testcase}.txt"

        mkdir -p "$final_dir"

        # 删除上一次实验结果
        rm -f "$final_dir"/*.csv
        rm -f "$final_dir"/*.xml
        rm -f "$output_file_real"

        if ! ./ns3 run --no-build --cwd="$final_dir" \
          "$testcase \
          --Deephir_threshold=$Deephir_threshold \
          --algorithm_name=BMS \
          --change_time_us=$change_time_us" \
          > "$output_file_real" 2>&1
        then
          echo "ERROR: $testcase BMS change_time_us=${change_time_us}us threshold=${Deephir_threshold}M 运行失败"
          echo "日志位置：$output_file_real"
          return 1
        fi

        echo "Finished BMS change_time_us=${change_time_us}us threshold=${Deephir_threshold}M"
      done
    done

  else
    echo "ERROR: runTest11 只支持 pbs 或 BMS，当前参数为：$algorithm"
    return 1
  fi

  echo "Finished testcase $testcase at $(date)"
}


runTest9()
{
  local case_id="${testcase#hybrid-buffer-test-}"
  local final_dir
  local output_file_real
  local status

  echo "testcase=$testcase"
  echo "algorithm=$algorithm"
  echo "TRAFFIC_GEN_DIR=$TRAFFIC_GEN_DIR"

  # 检查流量目录
  if [[ ! -d "$TRAFFIC_GEN_DIR" ]]; then
    echo "错误：TrafficGen目录不存在：$TRAFFIC_GEN_DIR"
    return 1
  fi

  # 检查两份流量文件
  if [[ ! -f "$TRAFFIC_GEN_DIR/Generated/traffic_web.txt" ]]; then
    echo "错误：缺少WebSearch流量文件："
    echo "$TRAFFIC_GEN_DIR/Generated/traffic_web.txt"
    return 1
  fi

  if [[ ! -f "$TRAFFIC_GEN_DIR/Generated/traffic_fbhdp.txt" ]]; then
    echo "错误：缺少Hadoop流量文件："
    echo "$TRAFFIC_GEN_DIR/Generated/traffic_fbhdp.txt"
    return 1
  fi

  echo "Starting testcase $testcase at $(date)"

  # PBS
  if [[ "$algorithm" == "pbs" || "$algorithm" == "both" ]]; then
    final_dir="${OUTPUT_DIR}/pbs/${case_id}"
    output_file_real="${final_dir}/${testcase}.txt"

    mkdir -p "$final_dir"
    rm -f "$final_dir"/*.csv

    echo "Starting testcase $testcase with pbs at $(date)"
    echo "./ns3 run --cwd=\"$final_dir\" \"$testcase --Deephir_threshold=1 --algorithm_name=pbs --traffic_gen_dir=$TRAFFIC_GEN_DIR\""
    ./ns3 run --no-build --cwd="$final_dir" \
      "$testcase \
      --Deephir_threshold=1 \
      --algorithm_name=pbs \
      --traffic_gen_dir=$TRAFFIC_GEN_DIR" \
      > "$output_file_real" 2>&1

    status=$?

    if [[ $status -ne 0 ]]; then
      echo "ERROR：PBS运行失败，退出码=$status"
      echo "日志位置：$output_file_real"
      return $status
    fi

    echo "Finished pbs at $(date)"
  fi

  # BMS：依次运行5个阈值
  if [[ "$algorithm" == "BMS" || "$algorithm" == "both" ]]; then
    for Deephir_threshold in 0.2 0.5 1.0 2.0 4.0
    do
      final_dir="${OUTPUT_DIR}/BMS/${case_id}/${Deephir_threshold}M"
      output_file_real="${final_dir}/${testcase}.txt"

      mkdir -p "$final_dir"
      rm -f "$final_dir"/*.csv

      echo "Starting testcase $testcase with Deephir_threshold=$Deephir_threshold at $(date)"

      ./ns3 run --no-build --cwd="$final_dir" \
        "$testcase \
        --Deephir_threshold=$Deephir_threshold \
        --algorithm_name=BMS \
        --traffic_gen_dir=$TRAFFIC_GEN_DIR" \
        > "$output_file_real" 2>&1

      status=$?

      if [[ $status -ne 0 ]]; then
        echo "ERROR：BMS运行失败"
        echo "阈值：$Deephir_threshold"
        echo "退出码：$status"
        echo "日志位置：$output_file_real"
        return $status
      fi

      echo "Finished threshold=$Deephir_threshold at $(date)"
    done
  fi

  echo "Finished testcase $testcase at $(date)"

  # 暂时先不自动画图，确保仿真数据完整生成
  # cd "$PLOT_DIR" || return 1
  # python3 TruePlot.py "$case_id"
}

run()
{
  OUTPUT_DIR_real="${OUTPUT_DIR}/${algorithm}"
  new_output_real="${OUTPUT_DIR_real}/${testcase:19:6}"
  output_file_real="${new_output_real}/${testcase}.txt"

  echo "Starting testcase $testcase at $(date)"

  mkdir -p "$new_output_real"

  rm -f "$new_output_real"/*.csv

if [[ "$testcase" == "hybrid-buffer-test-tc2-01" ||
      "$testcase" == "hybrid-buffer-test-tc2-02" ||
      "$testcase" == "hybrid-buffer-test-tc2-03" ||
      "$testcase" == "hybrid-buffer-test-tc2-04" ||
      "$testcase" == "hybrid-buffer-test-tc2-09" ||
      "$testcase" == "hybrid-buffer-test-tc2-10" ||
      "$testcase" == "hybrid-buffer-test-tc2-11" ||
      "$testcase" == "hybrid-buffer-test-tc2-12" ]]; then

  ./ns3 run --no-build --cwd="$new_output_real" \
    "$testcase --algorithm_name=$algorithm --traffic_gen_dir=$TRAFFIC_GEN_DIR" \
    > "$output_file_real" 2>&1

else

  ./ns3 run --no-build --cwd="$new_output_real" \
    "$testcase --algorithm_name=$algorithm" \
    > "$output_file_real" 2>&1

fi

  status=$?

  if [[ $status -ne 0 ]]; then
    echo "ERROR: $testcase 运行失败，退出码：$status"
    echo "日志位置：$output_file_real"
    return $status
  fi

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
  python3 traffic_gen_wk.py "$@"
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
  python3 traffic_gen.py "$@"
}
draw_basic_for_algorithm()
{
    local draw_algorithm="$1"
    local case_id
    cd "$PLOT_DIR" || return 1

    for case_id in tc2-05 tc2-06 tc2-07
    do
        echo "正在绘制 $case_id"
        if [[ "$draw_algorithm" == "BMS" ||
              "$draw_algorithm" == "both" ]]; then
            python3 ploting_sigle.py "$case_id" BMS || return 1
        fi

        if [[ "$draw_algorithm" == "pbs" ||
              "$draw_algorithm" == "both" ]]; then
            python3 ploting_sigle.py "$case_id" pbs || return 1
        fi

        if [[ "$draw_algorithm" == "both" ]]; then
            python3 ploting_merge.py "$case_id" || return 1
        fi

        echo "$case_id 绘制完成"
    done
}


################################################################################
# 绘制 tc2-05 到 tc2-09
draw_all_for_algorithm()
{
    local draw_algorithm="$1"

    draw_basic_for_algorithm "$draw_algorithm" || return 1

    cd "$PLOT_DIR" || return 1

    echo "正在绘制 tc2-08"

    # 当前原脚本只绘制了 tc2-08 的 PBS 结果
    if [[ "$draw_algorithm" == "pbs" ||
          "$draw_algorithm" == "both" ]]; then
        python3 ploting_sigle.py tc2-08 pbs || return 1
    fi

    if [[ "$draw_algorithm" == "BMS" ]]; then
        echo "提示：当前 tc2-08 的 BMS 数据包含阈值和速率两层目录，"
        echo "      原 ploting_sigle.py 暂未确认是否支持该目录结构。"
    fi

    echo "正在绘制 tc2-09"
    python3 TruePlot.py || return 1

    echo "全部图绘制完成"
}

###############################################################################################
#一键用两种算法（pbs和deephir）跑tc2-05到tc2-07，典型场景、多突发、多拥塞，同时画图
run_and_draw_basic()
{
    local alg
    local case_id
    local prefix="hybrid-buffer-test-"
    echo "开始运行基础场景：tc2-05 到 tc2-07"
    for alg in BMS pbs
    do
        algorithm="$alg"

        echo "当前算法：$algorithm"

        for case_id in tc2-05 tc2-06 tc2-07
        do
            testcase="${prefix}${case_id}"

            echo "正在运行 $testcase，算法：$algorithm"

            if ! run; then
                echo "错误：$testcase 使用 $algorithm 运行失败"
                return 1
            fi

            echo "$testcase 使用 $algorithm 运行完成"
        done
    done

    echo "基础场景全部运行完成，开始绘图"

    draw_basic_for_algorithm both || return 1
}

###############################################################################################
#一键用两种算法（pbs和deephir）跑从tc2-05到tc2-09所有的测试用例，同时一键画所有的图
run_and_draw_all()
{
    local requested_algorithm="$algorithm"
    local case_id
    local alg
    local prefix="hybrid-buffer-test-"
    local algorithm_list=()

    case "$requested_algorithm" in
        BMS)
            algorithm_list=("BMS")
            ;;
        pbs)
            algorithm_list=("pbs")
            ;;
        both)
            algorithm_list=("BMS" "pbs")
            ;;
        *)
            echo "错误：算法只支持 pbs、BMS 或 both"
            return 1
            ;;
    esac

    echo "开始运行 tc2-05 到 tc2-09"
    echo "指定算法：$requested_algorithm"

    for alg in "${algorithm_list[@]}"
    do
        algorithm="$alg"

        echo "=================================================="
        echo "当前算法：$algorithm"
        echo "=================================================="

        # tc2-05 到 tc2-07
        for case_id in tc2-05 tc2-06 tc2-07
        do
            testcase="${prefix}${case_id}"

            echo "正在运行 $testcase，算法：$algorithm"

            if ! run; then
                echo "错误：$testcase 使用 $algorithm 运行失败"
                return 1
            fi

            echo "$testcase 使用 $algorithm 运行完成"
        done

        # tc2-08
        testcase="${prefix}tc2-08"

        echo "正在运行 $testcase，算法：$algorithm"

        if ! runTest8; then
            echo "错误：$testcase 使用 $algorithm 运行失败"
            return 1
        fi

        echo "$testcase 使用 $algorithm 运行完成"

        # tc2-09
        testcase="${prefix}tc2-09"

        echo "正在运行 $testcase，算法：$algorithm"

        if ! runTest9; then
            echo "错误：$testcase 使用 $algorithm 运行失败"
            return 1
        fi

        echo "$testcase 使用 $algorithm 运行完成"
    done

    echo "所有仿真实验运行完成，开始绘图"

    draw_all_for_algorithm "$requested_algorithm" || return 1
}

draw_all()
{
    echo "开始绘制所有已有实验数据"

    draw_all_for_algorithm both || return 1
}

case "$1" in
    help)
        echohelp
        ;;

    clean)
        cleanFile
        ;;

    kill)
        killfunc
        ;;

    plotall)
        plotAll
        ;;

    runTest8)
        runTest8
        ;;

    runTest9)
        runTest9
        ;;

    run)
        run
        ;;

    show)
        showrun
        ;;

    TrafficGenWK)
        TrafficGen_wk "${@:2}"
        ;;

    TrafficGen)
        TrafficGen_original "${@:2}"
        ;;

    run_and_draw_all)
        run_and_draw_all
        ;;

    run_and_draw_basic)
        run_and_draw_basic
        ;;

    draw_all)
        draw_all
        ;;

    *)
        echo "错误：未知命令：$1"
        echo
        echo "支持的命令："
        echo "  run"
        echo "  runTest8"
        echo "  runTest9"
        echo "  run_and_draw_all"
        echo "  run_and_draw_basic"
        echo "  draw_all"
        echo "  TrafficGen"
        echo "  TrafficGenWK"
        echo "  clean"
        echo "  kill"
        echo "  show"
        exit 1
        ;;
esac

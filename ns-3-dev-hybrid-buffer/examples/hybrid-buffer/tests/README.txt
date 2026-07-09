    //************* 交换机配置 ***************
    uint32_t port = packet->GetMmuUsedPort();         // 端口号
    uint32_t qIndex = packet->GetMmuUsedQueueId();    // 队列号
    uint32_t priority = packet->GetMmuUsedPriority(); // 优先级
    uint32_t pktSize = packet->GetSize() + IPV4_INPUT_PKTSIZE_CORRECTION; // 包长

    uint64_t S = m_onChipBufferSize;                              // SRAM总缓存
    double D = m_offChipBuffer->GetDramBandwidth();             // DRAM总带宽
    uint64_t wcacheSize = m_offChipBuffer->GetWcacheSize();     // wcache总容量
    uint64_t wcacheUsed = m_offChipBuffer->GetWcacheUsed();     // wcache使用量

    //************* 缓存状态 ***************
    uint64_t qlen = m_qUsed[port][priority][qIndex];   // 当前队列总占用，包括 SRAM 和 DRAM
    uint64_t QiS = UsedSram_Size_Cycle[port][priority][qIndex];  // 当前队列位于SRAM中的数据量
    uint64_t QiD = qlen - QiS;  // 当前队列位于DRAM中的数据量
    bool isMixed = QiS > 0 && QiD > 0;

    uint64_t Sr = m_onChipBufferRemain;    // SRAM剩余容量
    double dtThreshold = DT_alpha * Sr;  //DT_Threshold = DT_alpha × SRAM剩余容量

    double Dr = Dr_EWMA;    // 当前DRAM剩余带宽
    bool hasWcacheSpace = wcacheUsed <= wcacheSize && (wcacheSize - wcacheUsed) >= pktSize;  //是否还有写wcache的空间
    bool hasDramSpace = m_offChipBuffer->GetDramRemain() >= pktSize;   //是否还有Dram的空间

    //************* 流量状态 ***************
    Packet_Size_Cycle[port][priority][qIndex] += pktSize; //  统计当前周期总到达数据量
    double lambdaLast = Packet_Size_Cycle[port][priority][qIndex] * 8.0 / (Simulator::Now() - simulation_start[port][priority][qIndex]).GetNanoSeconds();  // 上周期的流量到达速率
    double lambdaEwma; // 当前周期的流量到达率的指数加权移动平均值
    uint64_t inBytes; // 估计的下周期到达数据量 lambda(t+1) * T(t+1)
    uint64_t outBytesFromSram, outBytesMax; // 估计的下周期排出数据量 (估计), 下周期最大排出数据量 mu * T(t+1)
    uint64_t deltaQiS; // 估计的下周期SRAM队列长度变化量 arrivalBytes - serviceBytesFromSRAM
    double DTnext; // 估计的下周期末动态阈值 DT(t+1) 

    //************* 决策相关 ***************
    BmResult bmResult = BmResult(DROP);  //当前存储决策
    double cycleTime =(Simulator::Now() - simulation_start[port][priority][qIndex]) .GetNanoSeconds();  // 当前周期已经经过的实际时间
    double U1Ss = 0.0, U2Ss = 0.0, U1Ds = 0.0, U2Ds = 0.0, U_Sstar, U_Dstar, U_star, deltaU; // 上一周期的实际效用
    double MD, newT;
    double U_sram_1, U_sram_2, U_sram, U_dram_1, U_dram_2, U_dram; // 当前周期的效用

        //******************* 保存当前周期结束状态 ******************/
        simulation_start[port][priority][qIndex] = Simulator::Now(); // 周期结束时间
        m_Cost_ETC[port][priority][qIndex] = NanoSeconds(newT);
    
        Sr_last[port][priority][qIndex] = Sr; //周期结束时的Sr
        qlen_last[port][priority][qIndex] = qlen; //周期结束时的Sr
        Dr_last[port][priority][qIndex] = Dr; //周期结束时的Dr
        Qis_last[port][priority][qIndex] = QiS; //周期结束时的QiS，i.e., SRAM队列长度
    
        ReadSram_Size_Cycle[port][priority][qIndex] = 0; // 下一周期的SRAM出数据量，用于计算实际效用 U_2S*
        WriteDram_Size_Cycle[port][priority][qIndex] = 0; // 用于计算下一周期的DRAM写入速度
        Packet_Size_Cycle[port][priority][qIndex] = 0;
    
        T_seq[port][priority][qIndex] += 1; 
        drop_real_per_period[port][priority][qIndex] = 0; //每周期的实际丢包数



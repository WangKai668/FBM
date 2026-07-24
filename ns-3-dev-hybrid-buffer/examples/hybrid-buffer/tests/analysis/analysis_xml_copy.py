#!/usr/bin/env python3
import xml.etree.ElementTree as ET
import os
import sys

def get_xml_path(test_id, type):
    """根据测试编号自动构建XML文件路径"""
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(
        base_dir,
        "data",
        "BMS",
        test_id,
        type,
        f"flow-monitor-test-{test_id}.xml"
    )



# def get_xml_path(test_id, threshold, flow_rate):
#     """构建BMS实验XML路径"""
#     base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
#     return os.path.join(
#         base_dir,
#         "data",
#         "BMS",
#         test_id,
#         threshold,
#         str(flow_rate),
#         f"flow-monitor-test-{test_id}.xml"
#     )


def get_xml_path_pbs(test_id):
    """根据测试编号自动构建XML文件路径"""
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(
        base_dir,
        "data",
        "pbs",
        test_id,
        f"flow-monitor-test-{test_id}.xml"
    )



# def get_xml_path_pbs(test_id, flow_rate):
#     """构建PBS实验XML路径"""
#     base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
#     return os.path.join(
#         base_dir,
#         "data",
#         "pbs",
#         test_id,
#         str(flow_rate),
#         f"flow-monitor-test-{test_id}.xml"
#     )



def parse_flowmonitor_xml(xml_file):
    """解析FlowMonitor XML文件并计算吞吐量和FCT"""
    tree = ET.parse(xml_file)
    root = tree.getroot()
    
    # 获取所有流统计信息
    flow_stats = root.find('FlowStats')
    flows = {}
    for flow in flow_stats.findall('Flow'):
        flow_id = int(flow.get('flowId'))
        flows[flow_id] = {
            'tx_bytes': int(flow.get('txBytes')),
            'rx_bytes': int(flow.get('rxBytes')),
            'tx_packets': int(flow.get('txPackets')),
            'rx_packets': int(flow.get('rxPackets')),
            'time_first_tx': float(flow.get('timeFirstTxPacket').replace('ns', '')) / 1e9,
            'time_last_rx': float(flow.get('timeLastRxPacket').replace('ns', '')) / 1e9,
            # 移除原有的lostPackets，改为计算得出
        }
    
    # 获取流分类信息
    ipv4_classifier = root.find('Ipv4FlowClassifier')
    for flow in ipv4_classifier.findall('Flow'):
        flow_id = int(flow.get('flowId'))
        flows[flow_id].update({
            'src_addr': flow.get('sourceAddress'),
            'dst_addr': flow.get('destinationAddress'),
            'src_port': flow.get('sourcePort'),
            'dst_port': flow.get('destinationPort'),
            'protocol': 'TCP' if flow.get('protocol') == '6' else 'UDP'
        })
    
    return flows

def calculate_metrics(flows):
    """计算吞吐量和FCT"""
    results = []
    for flow_id, flow in flows.items():
        # 计算流持续时间(FCT) - 单位秒
        fct = flow['time_last_rx'] - flow['time_first_tx']
        
        # 计算吞吐量 - 单位Mbps
        throughput = (flow['rx_bytes'] * 8) / (fct * 1e6) if fct > 0 else 0
        
        # 计算丢失包数
        lost_packets = flow['tx_packets'] - flow['rx_packets']
        
        results.append({
            'flow_id': flow_id,
            'src': f"{flow['src_addr']}:{flow['src_port']}",
            'dst': f"{flow['dst_addr']}:{flow['dst_port']}",
            'protocol': flow['protocol'],
            'tx_bytes': flow['tx_bytes'],
            'rx_bytes': flow['rx_bytes'],
            'tx_packets': flow['tx_packets'],
            'rx_packets': flow['rx_packets'],
            'lost_packets': lost_packets,
            'fct_sec': f"{fct:.6f}",
            'throughput_mbps': f"{throughput:.2f}"
        })
    
    return sorted(results, key=lambda x: x['flow_id'])

def save_results(results, output_file):
    """将结果保存到文件"""
    with open(output_file, 'w') as f:
        f.write("Flow Performance Analysis:\n")
        f.write("-" * 135 + "\n")
        f.write(f"{'FlowID':<6} | {'Source':<20} | {'Destination':<20} | {'Proto':<6} | "
                f"{'TxBytes':<10} | {'RxBytes':<10} | {'TxPkts':<8} | {'RxPkts':<8} | {'Loss':<6} | "
                f"{'FCT(s)':<10} | {'Throughput(Mbps)':<15}\n")
        f.write("-" * 135 + "\n")
        
        for flow in results:
            f.write(f"{flow['flow_id']:<6} | {flow['src']:<20} | {flow['dst']:<20} | {flow['protocol']:<6} | "
                    f"{flow['tx_bytes']:<10} | {flow['rx_bytes']:<10} | {flow['tx_packets']:<8} | {flow['rx_packets']:<8} | "
                    f"{flow['lost_packets']:<6} | {flow['fct_sec']:<10} | {flow['throughput_mbps']:<15}\n")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <test_id> <type> (e.g. tc2-04 pbs)")
        print("Note: For BMS type, it will process all bandwidth types (0.2M to 4.0M)")
        sys.exit(1)
    
    test_id = sys.argv[1]
    data_type = sys.argv[2]

    if data_type == "BMS":
        type_list = ['0.2M', '0.5M', '1.0M', '2.0M', '4.0M']
        for bw_type in type_list:
            xml_file = get_xml_path(test_id, bw_type)
            output_file = os.path.join(os.path.dirname(xml_file), f"flow-analysis-{test_id}.txt")  # {test_id}
            
            if not os.path.exists(xml_file):
                print(f"Warning: XML file not found at {xml_file}, skipping...")
                continue
            
            print(f"Processing {test_id} with bandwidth {bw_type}...")
            print(f"Input XML: {xml_file}")
            print(f"Output TXT: {output_file}")
            
            flows = parse_flowmonitor_xml(xml_file)
            results = calculate_metrics(flows)
            save_results(results, output_file)
            
            print(f"Analysis completed. Results saved to {output_file}\n")
    elif data_type == "pbs":
        xml_file = get_xml_path_pbs(test_id)
        output_file = os.path.join(os.path.dirname(xml_file), f"flow-analysis-{test_id}.txt")
            
        if not os.path.exists(xml_file):
            print(f"Error: XML file not found at {xml_file}")
            sys.exit(1)
        
        print(f"Processing {test_id}...")
        print(f"Input XML: {xml_file}")
        print(f"Output TXT: {output_file}")
        
        flows = parse_flowmonitor_xml(xml_file)
        results = calculate_metrics(flows)
        save_results(results, output_file)
            
        print(f"Analysis completed. Results saved to {output_file}")
    else:
        print(f"Error: Unknown data type '{data_type}'. Use 'BMS' or 'pbs'.")
        sys.exit(1)
   
    # if data_type == "BMS":
    #     threshold_list = ['0.2M', '0.5M', '1.0M', '2.0M', '4.0M']
    #     flow_rate_list = [100, 200, 300, 400, 500, 600, 700, 800, 900]

    #     for threshold in threshold_list:
    #         for flow_rate in flow_rate_list:
    #             xml_file = get_xml_path(test_id, threshold, flow_rate)

    #             output_file = os.path.join(
    #                 os.path.dirname(xml_file),
    #                 f"flow-analysis-{test_id}.txt"
    #             )

    #             if not os.path.exists(xml_file):
    #                 print(f"Warning: XML file not found at {xml_file}, skipping...")
    #                 continue

    #             print(
    #                 f"Processing {test_id}, "
    #                 f"threshold={threshold}, flow_rate={flow_rate}Gbps..."
    #             )
    #             print(f"Input XML: {xml_file}")
    #             print(f"Output TXT: {output_file}")

    #             flows = parse_flowmonitor_xml(xml_file)
    #             results = calculate_metrics(flows)
    #             save_results(results, output_file)

    #             print(f"Analysis completed. Results saved to {output_file}\n")

    # elif data_type == "pbs":
    #     flow_rate_list = [100, 200, 300, 400, 500, 600, 700, 800, 900]

    #     for flow_rate in flow_rate_list:
    #         xml_file = get_xml_path_pbs(test_id, flow_rate)

    #         output_file = os.path.join(
    #             os.path.dirname(xml_file),
    #             f"flow-analysis-{test_id}.txt"
    #         )

    #         if not os.path.exists(xml_file):
    #             print(f"Warning: XML file not found at {xml_file}, skipping...")
    #             continue

    #         print(f"Processing {test_id}, flow_rate={flow_rate}Gbps...")
    #         print(f"Input XML: {xml_file}")
    #         print(f"Output TXT: {output_file}")

    #         flows = parse_flowmonitor_xml(xml_file)
    #         results = calculate_metrics(flows)
    #         save_results(results, output_file)

    #         print(f"Analysis completed. Results saved to {output_file}\n")

    # else:
    #     print(f"Error: Unknown data type '{data_type}'. Use 'BMS' or 'pbs'.")
    #     sys.exit(1)
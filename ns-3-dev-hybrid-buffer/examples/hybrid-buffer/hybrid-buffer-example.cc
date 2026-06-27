/*
 * Copyright (c) 2022 Xi'an Jiaotong University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Yuqi Liu <liuyuqi526@126.com>
 */

//
// Network topology
//
//  n1
//     \ 10 Mb/s, 1ms
//      \          10Mb/s, 1ms
//       n0 -------------------------n3
//      /
//     / 10 Mb/s, 1ms
//   n2
//
// - all net devices are reorder-point-to-point net devices
// - all links are point-to-point links with indicated one-way BW/delay
// - UDP flows from n1 to n3, and from n2 to n3
// - DropTail queues with backpressure from NetDeviceQueueInterface

#include "helper/star-sim-helper.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridBufferExample");

int
main(int argc, char* argv[])
{
    LogComponentEnable("HybridBufferExample", LOG_LEVEL_ALL);
    LogComponentEnable("SimHelper", LOG_LEVEL_ALL);
    LogComponentEnable("StarSimHelper", LOG_LEVEL_ALL);
    // LogComponentEnable("SwitchMmu", LOG_LEVEL_ALL);
    // LogComponentEnable("OffChipBuffer", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointReorderNetDevice", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointReorderHelper", LOG_LEVEL_ALL);
    // LogComponentEnable("PrioQueueDisc", ns3::LOG_LEVEL_ALL);
    // LogComponentEnable("DrrQueueDisc", LOG_LEVEL_ALL);
    // LogComponentEnable("FifoQueueDisc", LOG_LEVEL_ALL);
    // LogComponentEnable("QueueDisc", LOG_LEVEL_ALL);
    // LogComponentEnable("FiveTuplePacketFilter", LOG_LEVEL_ALL);
    // LogComponentEnable("TrafficControlLayer", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    hb::StarSimHelper simHelper("example");

    simHelper.EnableHbmThroughputTracing();
    simHelper.EnableBufferUsageTracing();

    simHelper.Run();

    return 0;
}

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
 * Authors: Shunlei Yang <yxlzqmysl0405@stu.xjtu.edu.cn>
 */

#include "ns3/application-module.h"
#include "ns3/core-modle.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/switch-mmu-helper.h"

/**
 * Default Network Topology
 *
 *        10.1.1.0
 * n0 -----switch 0------- n1
 *      point-to-point
 *
 * (node means node without switchmmu,
 *  while switch means node with switchmmu.)
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SwitchMmuTestScript");

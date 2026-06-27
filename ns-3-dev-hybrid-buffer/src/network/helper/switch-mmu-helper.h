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

#ifndef SWITCH_MMU_HELPER_H
#define SWITCH_MMU_HELPER_H

#include "ns3/node-container.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/switch-mmu.h"

namespace ns3
{

class Node;

/**
 * \ingroup network
 *
 * \brief Switch MMU Helper classes
 *
 * only enable Install.
 */
class SwitchMmuHelper : public Object
{
  public:
    /**
     * Constructor: Create a new SwitchMmuHelper.
     */
    SwitchMmuHelper();

    /**
     * Destroy a SwitchMmuHelper.
     */
    virtual ~SwitchMmuHelper();

    /**
     * Install SwitchMmu at a Node.
     */
    Ptr<SwitchMmu> Install(Ptr<Node> node) const;

    /**
     * Install SwitchMmu for every Node in NodeContainer.
     */
    // TODO: return a switch mmu container
    void Install(NodeContainer c) const;
};

} // namespace ns3

#endif /* SWITCH_MMU_HELPER_H */

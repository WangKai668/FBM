#ifndef HYBRID_BUFFER_FULLMESH_STAR_SIMULATION_HELPER_H
#define HYBRID_BUFFER_FULLMESH_STAR_SIMULATION_HELPER_H

#include "star-sim-helper.h"

namespace ns3
{

namespace hb
{

class FullmeshStarSimHelper : public StarSimHelper
{
  public:
    FullmeshStarSimHelper(std::string simName, Time start = Seconds(1), Time stop = Seconds(1.2));
    ~FullmeshStarSimHelper() override;
    void CreateTraffic() override;
    void SetUpFullMeshTraffic(std::vector<std::vector<int>>* fullmeshSocket, std::vector<std::vector<int>>* fullmeshWeight);
    void SetPktSize(uint32_t size);
  protected:
    std::vector<std::vector<int>> m_fullmeshSocket;
    std::vector<std::vector<int>> m_fullmeshWeight;
    uint32_t m_pktSize;
};

} // namespace hb

} // namespace ns3
#endif

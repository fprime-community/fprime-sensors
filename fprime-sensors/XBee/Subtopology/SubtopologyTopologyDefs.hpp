// ======================================================================
// \title  SubtopologyTopologyDefs.hpp
// \brief subtopology definitions header
//
// ======================================================================
#ifndef XBee_SubtopologyTopologyDefs_hpp
#define XBee_SubtopologyTopologyDefs_hpp

#include <Fw/Logger/Logger.hpp>
#include <fprime-sensors/XBee/Subtopology/XBeeConfig/FppConstantsAc.hpp>
#include "XBeeConfig/XBeeSubtopologyConfig.hpp"

namespace XBee {

struct SubtopologyState {
    XBeeDevice device;
    U32 baud;
};

struct TopologyState {
    SubtopologyState xbee;
};
}  // namespace XBee
#endif  // XBee_SubtopologyTopologyDefs_hpp

// ======================================================================
// \title  SubtopologyTopologyDefs.hpp
// \brief subtopology definitions header
//
// ======================================================================
#ifndef Rfm9X_SubtopologyTopologyDefs_hpp
#define Rfm9X_SubtopologyTopologyDefs_hpp

#include <Fw/Logger/Logger.hpp>
#include "Rfm9XConfig/Rfm9XSubtopologyConfig.hpp"

namespace Rfm9X {
struct SubtopologyState {
    Rfm9XDevice device;
};

struct TopologyState {
    SubtopologyState rfm9X;
};
}  // namespace Rfm9X
#endif  // Rfm9X_SubtopologyTopologyDefs_hpp

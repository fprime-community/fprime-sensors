// ======================================================================
// \title  SubtopologyTopologyDefs.hpp
// \brief subtopology definitions header
//
// ======================================================================
#ifndef Bmp280_SubtopologyTopologyDefs_hpp
#define Bmp280_SubtopologyTopologyDefs_hpp

#include <Fw/Logger/Logger.hpp>
#include "Bmp280Config/Bmp280SubtopologyConfig.hpp"

namespace Bmp280 {
    struct SubtopologyState {
        BmpDevice device;
    };

    struct TopologyState {
        SubtopologyState bmp;
    };
}
#endif // Bmp280_SubtopologyTopologyDefs_hpp 
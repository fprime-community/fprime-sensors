// ======================================================================
// \title  SubtopologyTopologyDefs.hpp
// \brief required header file containing the required definitions for the subtopology autocoder
// ======================================================================
#include <Fw/Types/MallocAllocator.hpp>
#include <NmeaGpsSubtopologyConfig/NmeaGpsSubtopologyConfig.hpp>
#include <fprime-sensors/NmeaGps/Subtopology/NmeaGpsSubtopologyConfig/FppConstantsAc.hpp>
#include <fprime-sensors/NmeaGps/Components/NmeaDetector/NmeaDetector.hpp>
#ifndef NmeaGps_SubtopologyTopologyDefs_hpp
#define NmeaGps_SubtopologyTopologyDefs_hpp

extern Fw::MallocAllocator mallocator;
namespace NmeaGps {
    struct SubtopologyState {
        NmeaGpsDevice device;
        U32 baud;
    };

    struct TopologyState {
        SubtopologyState gps;
    };
}

#endif // NmeaGps_SubtopologyTopologyDefs_hpp

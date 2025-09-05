// ======================================================================
// \title  XBeeSubtopologyConfig.hpp
// \brief required header file containing the required definitions for the subtopology autocoder
//
// ======================================================================
#ifndef XBee_XBeeSubtopologyConfig_hpp
#define XBee_XBeeSubtopologyConfig_hpp

#include <Fw/Types/BasicTypes.h>
#include "Fw/Types/MallocAllocator.hpp"

namespace XBee {
namespace Allocation {
extern Fw::MemAllocator& memAllocator;
}
}  // namespace XBee

struct XBeeDevice {
    const char* device;
    U32 baudRate;
};

#endif  // XBee_XBeeSubtopologyConfig_hpp

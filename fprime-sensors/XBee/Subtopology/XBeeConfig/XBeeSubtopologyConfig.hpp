// ======================================================================
// \title  XBeeSubtopologyConfig.hpp
// \brief required header file containing the required definitions for the subtopology autocoder
//
// ======================================================================
#ifndef XBee_XBeeSubtopologyConfig_hpp
#define XBee_XBeeSubtopologyConfig_hpp

#include <Fw/Logger/Logger.hpp>
#include "Fw/Types/MallocAllocator.hpp"

namespace XBee {
namespace Allocation {
extern Fw::MemAllocator& memAllocator;
}
}  // namespace XBee

// Configure how to identify a device here
// On Linux, it is a path to a UART device descriptor file
using XBeeDevice = const char*;

#endif  // XBee_XBeeSubtopologyConfig_hpp

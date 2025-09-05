#include "XBeeSubtopologyConfig.hpp"

namespace XBee {
namespace Allocation {
// This instance can be changed to use a different allocator in the XBee Subtopology
Fw::MallocAllocator mallocatorInstance;
Fw::MemAllocator& memAllocator = mallocatorInstance;
}  // namespace Allocation
}  // namespace XBee

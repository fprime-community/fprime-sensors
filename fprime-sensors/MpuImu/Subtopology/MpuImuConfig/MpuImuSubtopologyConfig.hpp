// ======================================================================
// \title  MpuImuSubtopologyConfig.hpp
// \brief required header file containing the required definitions for the subtopology autocoder
//
// ======================================================================
#ifndef MpuImu_MpuImuSubtopologyConfig_hpp
#define MpuImu_MpuImuSubtopologyConfig_hpp
#include <zephyr/drivers/i2c.h>
#undef EMPTY

using ImuDevice = struct i2c_dt_spec;

#endif // MpuImu_MpuImuSubtopologyConfig_hpp
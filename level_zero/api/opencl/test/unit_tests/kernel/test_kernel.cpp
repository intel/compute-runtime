/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include <level_zero/ze_api.h>

#include <CL/cl_ext.h>

namespace NEO {
namespace LEO {
namespace ult {

TEST(KernelIndirectAccessFlagToL0Tests, givenDeviceAccessFlagWhenConvertToL0ThenReturnsDeviceFlag) {
    EXPECT_EQ(ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE, Kernel::indirectAccessFlagToL0(CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL));
}

TEST(KernelIndirectAccessFlagToL0Tests, givenHostAccessFlagWhenConvertToL0ThenReturnsHostFlag) {
    EXPECT_EQ(ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST, Kernel::indirectAccessFlagToL0(CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL));
}

TEST(KernelIndirectAccessFlagToL0Tests, givenSharedAccessFlagWhenConvertToL0ThenReturnsSharedFlag) {
    EXPECT_EQ(ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED, Kernel::indirectAccessFlagToL0(CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL));
}

TEST(KernelSchedulingHintToL0Tests, givenRoundRobinPolicyWhenConvertToL0ThenReturnsRoundRobinFlag) {
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN, Kernel::schedulingHintToL0(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL));
}

TEST(KernelSchedulingHintToL0Tests, givenOldestFirstPolicyWhenConvertToL0ThenReturnsOldestFirstFlag) {
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST, Kernel::schedulingHintToL0(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL));
}

TEST(KernelSchedulingHintToL0Tests, givenStallBasedRoundRobinPolicyWhenConvertToL0ThenReturnsStallBasedFlag) {
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN, Kernel::schedulingHintToL0(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_STALL_BASED_ROUND_ROBIN_INTEL));
}

TEST(KernelSchedulingHintToL0Tests, givenAfterDependencyRoundRobinPolicyWhenConvertToL0ThenReturnsStallBasedFlag) {
    EXPECT_EQ(ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN, Kernel::schedulingHintToL0(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL));
}

} // namespace ult
} // namespace LEO
} // namespace NEO

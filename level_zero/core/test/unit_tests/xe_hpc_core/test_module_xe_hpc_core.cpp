/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using KernelPropertyTest = Test<DeviceFixture>;

HWTEST2_F(KernelPropertyTest, givenKernelExtendedPropertiesStructureWhenKernelPropertiesCalledThenPropertiesAreCorrectlySet, IsXeHpcCore) {
    ze_device_module_properties_t kernelProperties = {};
    ze_float_atomic_ext_properties_t kernelExtendedProperties = {};
    kernelExtendedProperties.stype = ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES;
    kernelProperties.pNext = &kernelExtendedProperties;
    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_TRUE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE);
    EXPECT_TRUE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_ADD);
    EXPECT_TRUE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    EXPECT_FALSE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_LOCAL_LOAD_STORE);
    EXPECT_FALSE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_LOCAL_ADD);
    EXPECT_FALSE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_LOCAL_MIN_MAX);

    EXPECT_TRUE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE);
    EXPECT_TRUE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_ADD);
    EXPECT_TRUE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    EXPECT_FALSE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_LOCAL_LOAD_STORE);
    EXPECT_FALSE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_LOCAL_ADD);
    EXPECT_FALSE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_LOCAL_MIN_MAX);

    EXPECT_TRUE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE);
    EXPECT_TRUE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_ADD);
    EXPECT_TRUE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    EXPECT_FALSE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_LOCAL_LOAD_STORE);
    EXPECT_FALSE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_LOCAL_ADD);
    EXPECT_FALSE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_LOCAL_MIN_MAX);
}

} // namespace ult
} // namespace L0
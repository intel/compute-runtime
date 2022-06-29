/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using KernelPropertyTest = Test<DeviceFixture>;

HWTEST2_F(KernelPropertyTest, givenReturnedKernelPropertiesThenExpectedDp4aSupportReturned, IsGen9) {
    ze_device_module_properties_t kernelProps = {};

    device->getKernelProperties(&kernelProps);
    EXPECT_EQ(0u, kernelProps.flags & ZE_DEVICE_MODULE_FLAG_DP4A);
}

HWTEST2_F(KernelPropertyTest, givenKernelExtendedPropertiesStructureWhenKernelPropertiesCalledThenPropertiesAreCorrectlySet, IsGen9) {
    ze_device_module_properties_t kernelProperties = {};
    ze_float_atomic_ext_properties_t kernelExtendedProperties = {};
    kernelExtendedProperties.stype = ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES;
    kernelProperties.pNext = &kernelExtendedProperties;
    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_EQ(0u, kernelExtendedProperties.fp16Flags);
    EXPECT_EQ(0u, kernelExtendedProperties.fp32Flags);
    EXPECT_EQ(0u, kernelExtendedProperties.fp64Flags);
}

} // namespace ult
} // namespace L0

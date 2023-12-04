/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using KernelPropertyTest = Test<DeviceFixture>;

HWTEST2_F(KernelPropertyTest, givenKernelExtendedPropertiesStructureWhenKernelPropertiesCalledThenPropertiesAreCorrectlySet, IsGen12LP) {
    ze_device_module_properties_t kernelProperties = {};
    ze_float_atomic_ext_properties_t kernelExtendedProperties = {};
    kernelExtendedProperties.stype = ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES;
    kernelProperties.pNext = &kernelExtendedProperties;
    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    const auto &fp16Properties = kernelExtendedProperties.fp16Flags;
    EXPECT_TRUE(fp16Properties & FpAtomicExtFlags::globalLoadStore);
    EXPECT_TRUE(fp16Properties & FpAtomicExtFlags::localLoadStore);
    EXPECT_TRUE(fp16Properties & FpAtomicExtFlags::globalMinMax);
    EXPECT_TRUE(fp16Properties & FpAtomicExtFlags::localMinMax);

    EXPECT_FALSE(fp16Properties & FpAtomicExtFlags::globalAdd);
    EXPECT_FALSE(fp16Properties & FpAtomicExtFlags::localAdd);

    const auto &fp32Properties = kernelExtendedProperties.fp32Flags;
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::globalLoadStore);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::localLoadStore);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::globalMinMax);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::localMinMax);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::globalAdd);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::localAdd);

    const auto &fp64Properties = kernelExtendedProperties.fp64Flags;
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::globalLoadStore);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::localLoadStore);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::globalMinMax);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::localMinMax);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::globalAdd);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::localAdd);
}

} // namespace ult
} // namespace L0

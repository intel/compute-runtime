/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using KernelPropertyTest = Test<DeviceFixture>;

HWTEST2_F(KernelPropertyTest, givenKernelExtendedPropertiesStructureWhenKernelPropertiesCalledThenPropertiesAreCorrectlySet, IsXeHpgCore) {
    ze_device_module_properties_t kernelProperties = {};
    ze_float_atomic_ext_properties_t kernelExtendedProperties = {};
    kernelExtendedProperties.stype = ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES;
    kernelProperties.pNext = &kernelExtendedProperties;
    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_FALSE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE);
    EXPECT_FALSE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_ADD);
    EXPECT_FALSE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    EXPECT_FALSE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_LOCAL_LOAD_STORE);
    EXPECT_FALSE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_LOCAL_ADD);
    EXPECT_FALSE(kernelExtendedProperties.fp16Flags & FP_ATOMIC_EXT_FLAG_LOCAL_MIN_MAX);

    EXPECT_FALSE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE);
    EXPECT_TRUE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_ADD);
    EXPECT_FALSE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    EXPECT_FALSE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_LOCAL_LOAD_STORE);
    EXPECT_FALSE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_LOCAL_ADD);
    EXPECT_FALSE(kernelExtendedProperties.fp32Flags & FP_ATOMIC_EXT_FLAG_LOCAL_MIN_MAX);

    EXPECT_FALSE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE);
    EXPECT_FALSE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_ADD);
    EXPECT_FALSE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    EXPECT_FALSE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_LOCAL_LOAD_STORE);
    EXPECT_FALSE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_LOCAL_ADD);
    EXPECT_FALSE(kernelExtendedProperties.fp64Flags & FP_ATOMIC_EXT_FLAG_LOCAL_MIN_MAX);
}

HWTEST2_F(KernelPropertyTest, givenDG2WhenGetInternalOptionsThenWriteBackBuildOptionIsSet, IsDG2) {
    auto pMockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(pMockCompilerInterface);
    MockModuleTranslationUnit moduleTu(this->device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(pMockCompilerInterface->inputInternalOptions.find("-cl-store-cache-default=7 -cl-load-cache-default=4"), std::string::npos);
}

} // namespace ult
} // namespace L0

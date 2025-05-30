/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"

#include "cl_api_tests.h"

#include <cstring>

using namespace NEO;

using clGetDeviceInfoTests = ApiTests;

namespace ULT {

static_assert(CL_DEVICE_IL_VERSION == CL_DEVICE_IL_VERSION_KHR, "Param values are different");

TEST_F(clGetDeviceInfoTests, givenNeoDeviceWhenAskedForSliceCountThenNumberOfSlicesIsReturned) {
    cl_device_info paramName = 0;
    size_t paramSize = 0;
    void *paramValue = nullptr;
    size_t paramRetSize = 0;

    size_t numSlices = 0;
    paramName = CL_DEVICE_SLICE_COUNT_INTEL;

    retVal = clGetDeviceInfo(
        testedClDevice,
        paramName,
        0,
        nullptr,
        &paramRetSize);

    EXPECT_EQ(sizeof(size_t), paramRetSize);
    paramSize = paramRetSize;
    paramValue = &numSlices;

    retVal = clGetDeviceInfo(
        testedClDevice,
        paramName,
        paramSize,
        paramValue,
        &paramRetSize);

    EXPECT_EQ(defaultHwInfo->gtSystemInfo.SliceCount, numSlices);
}

TEST_F(clGetDeviceInfoTests, GivenGpuDeviceWhenGettingDeviceInfoThenDeviceTypeGpuIsReturned) {
    cl_device_info paramName = 0;
    size_t paramSize = 0;
    void *paramValue = nullptr;
    size_t paramRetSize = 0;

    cl_device_type deviceType = CL_DEVICE_TYPE_CPU; // set to wrong value
    paramName = CL_DEVICE_TYPE;
    paramSize = sizeof(cl_device_type);
    paramValue = &deviceType;

    retVal = clGetDeviceInfo(
        testedClDevice,
        paramName,
        paramSize,
        paramValue,
        &paramRetSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_device_type>(CL_DEVICE_TYPE_GPU), deviceType);
}

TEST_F(clGetDeviceInfoTests, GivenNullDeviceWhenGettingDeviceInfoThenInvalidDeviceErrorIsReturned) {
    size_t paramRetSize = 0;

    retVal = clGetDeviceInfo(
        nullptr,
        CL_DEVICE_TYPE,
        0,
        nullptr,
        &paramRetSize);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clGetDeviceInfoTests, givenOpenCLDeviceWhenAskedForSupportedSvmTypeThenCorrectValueIsReturned) {

    cl_device_svm_capabilities svmCaps;

    retVal = clGetDeviceInfo(
        testedClDevice,
        CL_DEVICE_SVM_CAPABILITIES,
        sizeof(cl_device_svm_capabilities),
        &svmCaps,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const HardwareInfo &hwInfo = pDevice->getHardwareInfo();

    cl_device_svm_capabilities expectedCaps = 0;
    if (hwInfo.capabilityTable.ftrSupportsCoherency != 0) {
        expectedCaps = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER | CL_DEVICE_SVM_FINE_GRAIN_BUFFER | CL_DEVICE_SVM_ATOMICS;
    } else {
        expectedCaps = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;
    }
    EXPECT_EQ(svmCaps, expectedCaps);
}

TEST(clGetDeviceGlobalMemSizeTests, givenDebugFlagForGlobalMemSizePercentWhenAskedForGlobalMemSizeThenAdjustedGlobalMemSizeIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.set(100u);
    uint64_t globalMemSize100percent = 0u;

    auto hwInfo = *defaultHwInfo;

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    auto retVal = clGetDeviceInfo(
        pDevice.get(),
        CL_DEVICE_GLOBAL_MEM_SIZE,
        sizeof(uint64_t),
        &globalMemSize100percent,
        nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(globalMemSize100percent, 0u);

    debugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.set(50u);
    uint64_t globalMemSize50percent = 0u;

    hwInfo = *defaultHwInfo;

    pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    retVal = clGetDeviceInfo(
        pDevice.get(),
        CL_DEVICE_GLOBAL_MEM_SIZE,
        sizeof(uint64_t),
        &globalMemSize50percent,
        nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(globalMemSize50percent, 0u);

    EXPECT_EQ(globalMemSize100percent / 2u, globalMemSize50percent);
}

TEST(clGetDeviceFineGrainedTests, givenDebugFlagForFineGrainedOverrideWhenItIsUsedWithZeroThenNoFineGrainSupport) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceFineGrainedSVMSupport.set(0);
    cl_device_svm_capabilities svmCaps;

    auto hwInfo = *defaultHwInfo;

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    auto retVal = clGetDeviceInfo(
        pDevice.get(),
        CL_DEVICE_SVM_CAPABILITIES,
        sizeof(cl_device_svm_capabilities),
        &svmCaps,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_device_svm_capabilities expectedCaps = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;
    EXPECT_EQ(svmCaps, expectedCaps);
}

TEST(clGetDeviceFineGrainedTests, givenDebugFlagForFineGrainedOverrideWhenItIsUsedWithOneThenThereIsFineGrainSupport) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceFineGrainedSVMSupport.set(1);
    cl_device_svm_capabilities svmCaps;

    auto hwInfo = *defaultHwInfo;

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    auto retVal = clGetDeviceInfo(
        pDevice.get(),
        CL_DEVICE_SVM_CAPABILITIES,
        sizeof(cl_device_svm_capabilities),
        &svmCaps,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_device_svm_capabilities expectedCaps = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER | CL_DEVICE_SVM_FINE_GRAIN_BUFFER | CL_DEVICE_SVM_ATOMICS;
    EXPECT_EQ(svmCaps, expectedCaps);
}

TEST_F(clGetDeviceInfoTests, givenNeoDeviceWhenAskedForDriverVersionThenNeoIsReturned) {
    cl_device_info paramName = 0;
    size_t paramSize = 0;
    void *paramValue = nullptr;
    size_t paramRetSize = 0;

    cl_uint driverVersion = 0;
    paramName = CL_DEVICE_DRIVER_VERSION_INTEL;

    retVal = clGetDeviceInfo(
        testedClDevice,
        paramName,
        0,
        nullptr,
        &paramRetSize);

    EXPECT_EQ(sizeof(cl_uint), paramRetSize);
    paramSize = paramRetSize;
    paramValue = &driverVersion;

    retVal = clGetDeviceInfo(
        testedClDevice,
        paramName,
        paramSize,
        paramValue,
        &paramRetSize);

    EXPECT_EQ((cl_uint)CL_DEVICE_DRIVER_VERSION_INTEL_NEO1, driverVersion);
}

TEST_F(clGetDeviceInfoTests, GivenClDeviceExtensionsParamWhenGettingDeviceInfoThenAllExtensionsAreListed) {
    size_t paramRetSize = 0;

    cl_int retVal = clGetDeviceInfo(
        testedClDevice,
        CL_DEVICE_EXTENSIONS,
        0,
        nullptr,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramRetSize);

    auto paramValue = std::make_unique<char[]>(paramRetSize);

    retVal = clGetDeviceInfo(
        testedClDevice,
        CL_DEVICE_EXTENSIONS,
        paramRetSize,
        paramValue.get(),
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    std::string extensionString(paramValue.get());
    static const char *const supportedExtensions[] = {
        "cl_khr_byte_addressable_store ",
        "cl_khr_device_uuid ",
        "cl_khr_fp16 ",
        "cl_khr_global_int32_base_atomics ",
        "cl_khr_global_int32_extended_atomics ",
        "cl_khr_icd ",
        "cl_khr_local_int32_base_atomics ",
        "cl_khr_local_int32_extended_atomics ",
        "cl_intel_command_queue_families",
        "cl_intel_subgroups ",
        "cl_intel_required_subgroup_size ",
        "cl_intel_subgroups_short ",
        "cl_khr_spir ",
        "cl_intel_accelerator ",
        "cl_intel_driver_diagnostics ",
        "cl_khr_priority_hints ",
        "cl_khr_throttle_hints ",
        "cl_khr_create_command_queue ",
        "cl_intel_subgroups_char ",
        "cl_intel_subgroups_long ",
        "cl_khr_il_program ",
        "cl_khr_subgroup_extended_types ",
        "cl_khr_subgroup_non_uniform_vote ",
        "cl_khr_subgroup_ballot ",
        "cl_khr_subgroup_non_uniform_arithmetic ",
        "cl_khr_subgroup_shuffle ",
        "cl_khr_subgroup_shuffle_relative ",
        "cl_khr_subgroup_clustered_reduce ",
        "cl_intel_device_attribute_query ",
        "cl_khr_suggested_local_work_size ",
        "cl_intel_split_work_group_barrier "};

    for (auto extension : supportedExtensions) {
        auto foundOffset = extensionString.find(extension);
        EXPECT_TRUE(foundOffset != std::string::npos);
    }
}

TEST_F(clGetDeviceInfoTests, GivenClDeviceIlVersionParamWhenGettingDeviceInfoThenSpirv10To15IsReturned) {
    size_t paramRetSize = 0;

    cl_int retVal = clGetDeviceInfo(
        testedClDevice,
        CL_DEVICE_IL_VERSION,
        0,
        nullptr,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramRetSize);

    auto paramValue = std::make_unique<char[]>(paramRetSize);

    retVal = clGetDeviceInfo(
        testedClDevice,
        CL_DEVICE_IL_VERSION,
        paramRetSize,
        paramValue.get(),
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_STREQ("SPIR-V_1.5 SPIR-V_1.4 SPIR-V_1.3 SPIR-V_1.2 SPIR-V_1.1 SPIR-V_1.0 ", paramValue.get());
}

using matcherAtMostGen12lp = IsAtMostGfxCore<IGFX_GEN12LP_CORE>;
HWTEST2_F(clGetDeviceInfoTests, givenClDeviceSupportedThreadArbitrationPolicyIntelWhenCallClGetDeviceInfoThenProperArrayIsReturned, matcherAtMostGen12lp) {
    cl_device_info paramName = 0;
    cl_uint paramValue[3];
    size_t paramSize = sizeof(paramValue);
    size_t paramRetSize = 0;

    paramName = CL_DEVICE_SUPPORTED_THREAD_ARBITRATION_POLICY_INTEL;
    cl_uint expectedRetValue[] = {CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL, CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL};

    retVal = clGetDeviceInfo(
        testedClDevice,
        paramName,
        paramSize,
        paramValue,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(expectedRetValue), paramRetSize);
    EXPECT_TRUE(memcmp(expectedRetValue, paramValue, sizeof(expectedRetValue)) == 0);
}

HWTEST_F(clGetDeviceInfoTests, givenClDeviceSupportedThreadArbitrationPolicyIntelWhenThreadArbitrationPolicyChangeNotSupportedAndCallClGetDeviceInfoThenParamRetSizeIsZero) {
    auto &clGfxCoreHelper = this->pDevice->getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();
    if (clGfxCoreHelper.isSupportedKernelThreadArbitrationPolicy()) {
        GTEST_SKIP();
    }
    cl_device_info paramName = 0;
    cl_uint paramValue[3];
    size_t paramSize = sizeof(paramValue);
    size_t paramRetSize = 0;

    paramName = CL_DEVICE_SUPPORTED_THREAD_ARBITRATION_POLICY_INTEL;

    retVal = clGetDeviceInfo(
        testedClDevice,
        paramName,
        paramSize,
        paramValue,
        &paramRetSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, paramRetSize);
}

//------------------------------------------------------------------------------
struct GetDeviceInfoP : public ApiFixture<>,
                        public ::testing::TestWithParam<uint32_t /*cl_device_info*/> {
    void SetUp() override {
        param = GetParam();
        ApiFixture::setUp();
    }

    void TearDown() override {
        ApiFixture::tearDown();
    }

    cl_device_info param;
};

typedef GetDeviceInfoP GetDeviceInfoStr;

TEST_P(GetDeviceInfoStr, GivenStringTypeParamWhenGettingDeviceInfoThenSuccessIsReturned) {
    size_t paramRetSize = 0;

    cl_int retVal = clGetDeviceInfo(
        testedClDevice,
        param,
        0,
        nullptr,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramRetSize);

    auto paramValue = std::make_unique<char[]>(paramRetSize);

    retVal = clGetDeviceInfo(
        testedClDevice,
        param,
        paramRetSize,
        paramValue.get(),
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

static cl_device_info deviceInfoStrParams[] =
    {
        CL_DEVICE_BUILT_IN_KERNELS,
        CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED,
        CL_DEVICE_NAME,
        CL_DEVICE_OPENCL_C_VERSION,
        CL_DEVICE_PROFILE,
        CL_DEVICE_VENDOR,
        CL_DEVICE_VERSION,
        CL_DRIVER_VERSION};

INSTANTIATE_TEST_SUITE_P(
    api,
    GetDeviceInfoStr,
    testing::ValuesIn(deviceInfoStrParams));

typedef GetDeviceInfoP GetDeviceInfoVectorWidth;

TEST_P(GetDeviceInfoVectorWidth, GivenParamTypeVectorWhenGettingDeviceInfoThenSizeIsGreaterThanZeroAndValueIsGreaterThanZero) {
    cl_uint paramValue = 0;
    size_t paramRetSize = 0;

    auto retVal = clGetDeviceInfo(
        testedClDevice,
        param,
        0,
        nullptr,
        &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), paramRetSize);

    retVal = clGetDeviceInfo(
        testedClDevice,
        param,
        paramRetSize,
        &paramValue,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(paramValue, 0u);
}

cl_device_info devicePreferredVector[] = {
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,
    CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT};

INSTANTIATE_TEST_SUITE_P(
    api,
    GetDeviceInfoVectorWidth,
    testing::ValuesIn(devicePreferredVector));

TEST_F(clGetDeviceInfoTests, givenClDeviceWhenGetInfoForIntegerDotCapsThenCorrectValuesAreSet) {
    size_t paramRetSize = 0;
    cl_device_integer_dot_product_capabilities_khr integerDotCapabilities{};

    clGetDeviceInfo(
        testedClDevice,
        CL_DEVICE_INTEGER_DOT_PRODUCT_CAPABILITIES_KHR,
        sizeof(integerDotCapabilities),
        &integerDotCapabilities,
        &paramRetSize);
    auto &compilerHelper = pDevice->getDevice().getCompilerProductHelper();
    EXPECT_EQ(((integerDotCapabilities & CL_DEVICE_INTEGER_DOT_PRODUCT_INPUT_4x8BIT_KHR) && (integerDotCapabilities & CL_DEVICE_INTEGER_DOT_PRODUCT_INPUT_4x8BIT_PACKED_KHR)), compilerHelper.isDotIntegerProductExtensionSupported());
}

TEST_F(clGetDeviceInfoTests, givenClDeviceWhenGetInfoForIntegerDot8BitPropertiesThenCorrectValuesAreSet) {
    size_t paramRetSize = 0;
    cl_device_integer_dot_product_acceleration_properties_khr integerDotAccelerationProperties8Bit{};

    clGetDeviceInfo(
        testedClDevice,
        CL_DEVICE_INTEGER_DOT_PRODUCT_ACCELERATION_PROPERTIES_8BIT_KHR,
        sizeof(integerDotAccelerationProperties8Bit),
        &integerDotAccelerationProperties8Bit,
        &paramRetSize);
    auto &compilerHelper = pDevice->getDevice().getCompilerProductHelper();
    EXPECT_EQ(integerDotAccelerationProperties8Bit.accumulating_saturating_mixed_signedness_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8Bit.accumulating_saturating_signed_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8Bit.accumulating_saturating_unsigned_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8Bit.mixed_signedness_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8Bit.signed_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8Bit.unsigned_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
}

TEST_F(clGetDeviceInfoTests, givenClDeviceWhenGetInfoForIntegerDot8BitPackedPropertiesThenCorrectValuesAreSet) {
    size_t paramRetSize = 0;
    cl_device_integer_dot_product_acceleration_properties_khr integerDotAccelerationProperties8BitPacked{};

    clGetDeviceInfo(
        testedClDevice,
        CL_DEVICE_INTEGER_DOT_PRODUCT_ACCELERATION_PROPERTIES_4x8BIT_PACKED_KHR,
        sizeof(integerDotAccelerationProperties8BitPacked),
        &integerDotAccelerationProperties8BitPacked,
        &paramRetSize);
    auto &compilerHelper = pDevice->getDevice().getCompilerProductHelper();
    EXPECT_EQ(integerDotAccelerationProperties8BitPacked.accumulating_saturating_mixed_signedness_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8BitPacked.accumulating_saturating_signed_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8BitPacked.accumulating_saturating_unsigned_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8BitPacked.mixed_signedness_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8BitPacked.signed_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(integerDotAccelerationProperties8BitPacked.unsigned_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
}
} // namespace ULT

/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/test/unit_test/device/device_caps_test_utils.h"
#include "opencl/test/unit_test/fixtures/device_info_fixture.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "driver_version.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

struct DeviceGetCapsTest : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.ContextGroupSize.set(0);
        MockSipData::clearUseFlags();
        backupSipInitType = std::make_unique<VariableBackup<bool>>(&MockSipData::useMockSip, true);
    }
    void TearDown() override {
        MockSipData::clearUseFlags();
    }

    void verifyOpenclCAllVersions(MockClDevice &clDevice) {
        EXPECT_FALSE(clDevice.getDeviceInfo().openclCAllVersions.usesDynamicMem());

        for (auto &openclCVersion : clDevice.getDeviceInfo().openclCAllVersions) {
            EXPECT_STREQ("OpenCL C", openclCVersion.name);
        }

        auto openclCWithVersionIterator = clDevice.getDeviceInfo().openclCAllVersions.begin();

        EXPECT_EQ(CL_MAKE_VERSION(1u, 0u, 0u), openclCWithVersionIterator->version);
        EXPECT_EQ(CL_MAKE_VERSION(1u, 1u, 0u), (++openclCWithVersionIterator)->version);
        EXPECT_EQ(CL_MAKE_VERSION(1u, 2u, 0u), (++openclCWithVersionIterator)->version);

        if (clDevice.getEnabledClVersion() == 30) {
            EXPECT_EQ(CL_MAKE_VERSION(3u, 0u, 0u), (++openclCWithVersionIterator)->version);
        }

        EXPECT_EQ(clDevice.getDeviceInfo().openclCAllVersions.end(), ++openclCWithVersionIterator) << " versions count : " << clDevice.getDeviceInfo().openclCAllVersions.size();
    }

    void verifyOpenclCFeatures(MockClDevice &clDevice) {
        EXPECT_FALSE(clDevice.getDeviceInfo().openclCFeatures.usesDynamicMem());

        for (auto &openclCFeature : clDevice.getDeviceInfo().openclCFeatures) {
            EXPECT_EQ(CL_MAKE_VERSION(3u, 0u, 0u), openclCFeature.version);
        }

        auto &hwInfo = clDevice.getHardwareInfo();
        auto releaseHelper = clDevice.getDevice().getReleaseHelper();
        auto openclCFeatureIterator = clDevice.getDeviceInfo().openclCFeatures.begin();

        EXPECT_STREQ("__opencl_c_int64", openclCFeatureIterator->name);

        if (hwInfo.capabilityTable.supportsImages) {
            EXPECT_STREQ("__opencl_c_3d_image_writes", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_images", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_read_write_images", (++openclCFeatureIterator)->name);
        }
        if (hwInfo.capabilityTable.supportsOcl21Features) {
            EXPECT_STREQ("__opencl_c_atomic_order_acq_rel", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_atomic_order_seq_cst", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_atomic_scope_all_devices", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_atomic_scope_device", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_generic_address_space", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_program_scope_global_variables", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_work_group_collective_functions", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_subgroups", (++openclCFeatureIterator)->name);
            if (hwInfo.capabilityTable.supportsFloatAtomics) {
                EXPECT_STREQ("__opencl_c_ext_fp32_global_atomic_add", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp32_local_atomic_add", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp32_global_atomic_min_max", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp32_local_atomic_min_max", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp16_global_atomic_load_store", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp16_local_atomic_load_store", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp16_global_atomic_min_max", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp16_local_atomic_min_max", (++openclCFeatureIterator)->name);
            }
        }
        if (hwInfo.capabilityTable.ftrSupportsFP64) {
            EXPECT_STREQ("__opencl_c_fp64", (++openclCFeatureIterator)->name);
            if (hwInfo.capabilityTable.supportsOcl21Features && hwInfo.capabilityTable.supportsFloatAtomics) {
                EXPECT_STREQ("__opencl_c_ext_fp64_global_atomic_add", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp64_local_atomic_add", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp64_global_atomic_min_max", (++openclCFeatureIterator)->name);
                EXPECT_STREQ("__opencl_c_ext_fp64_local_atomic_min_max", (++openclCFeatureIterator)->name);
            }
        }
        if (clDevice.getDevice().getCompilerProductHelper().isDotIntegerProductExtensionSupported()) {
            EXPECT_STREQ("__opencl_c_integer_dot_product_input_4x8bit", (++openclCFeatureIterator)->name);
            EXPECT_STREQ("__opencl_c_integer_dot_product_input_4x8bit_packed", (++openclCFeatureIterator)->name);
        }
        verifyAnyRemainingOpenclCFeatures(releaseHelper, openclCFeatureIterator);
        EXPECT_EQ(clDevice.getDeviceInfo().openclCFeatures.end(), ++openclCFeatureIterator);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<VariableBackup<bool>> backupSipInitType;
};

TEST_F(DeviceGetCapsTest, WhenCreatingDeviceThenCapsArePopulatedCorrectly) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    const auto &sharedCaps = device->getSharedDeviceInfo();
    const auto &sysInfo = defaultHwInfo->gtSystemInfo;
    auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    auto &productHelper = device->getProductHelper();

    EXPECT_NE(nullptr, caps.builtInKernels);

    std::string strDriverName = caps.name;
    std::string strDeviceName = device->getClDeviceName();

    EXPECT_NE(std::string::npos, strDriverName.find(strDeviceName));

    EXPECT_NE(nullptr, caps.name);
    EXPECT_NE(nullptr, caps.vendor);
    EXPECT_NE(nullptr, caps.driverVersion);
    EXPECT_NE(nullptr, caps.profile);
    EXPECT_STREQ("OpenCL 3.0 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
    EXPECT_NE(0u, caps.numericClVersion);
    EXPECT_GT(caps.openclCAllVersions.size(), 0u);
    EXPECT_GT(caps.openclCFeatures.size(), 0u);
    EXPECT_EQ(caps.extensionsWithVersion.size(), 0u);
    EXPECT_STREQ("v2025-04-14-00", caps.latestConformanceVersionPassed);

    EXPECT_NE(nullptr, caps.spirVersions);
    EXPECT_NE(nullptr, caps.deviceExtensions);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), caps.deviceAvailable);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), caps.compilerAvailable);
    EXPECT_EQ(16u, caps.preferredVectorWidthChar);
    EXPECT_EQ(8u, caps.preferredVectorWidthShort);
    EXPECT_EQ(4u, caps.preferredVectorWidthInt);
    EXPECT_EQ(1u, caps.preferredVectorWidthLong);
    EXPECT_EQ(1u, caps.preferredVectorWidthFloat);
    EXPECT_EQ(8u, caps.preferredVectorWidthHalf);
    EXPECT_EQ(16u, caps.nativeVectorWidthChar);
    EXPECT_EQ(8u, caps.nativeVectorWidthShort);
    EXPECT_EQ(4u, caps.nativeVectorWidthInt);
    EXPECT_EQ(1u, caps.nativeVectorWidthLong);
    EXPECT_EQ(1u, caps.nativeVectorWidthFloat);
    EXPECT_EQ(8u, caps.nativeVectorWidthHalf);
    EXPECT_EQ(1u, caps.linkerAvailable);
    EXPECT_NE(0u, sharedCaps.globalMemCachelineSize);
    EXPECT_NE(0u, caps.globalMemCacheSize);
    EXPECT_LT(0u, sharedCaps.globalMemSize);
    EXPECT_EQ(sharedCaps.maxMemAllocSize, caps.maxConstantBufferSize);
    EXPECT_STREQ("SPIR-V_1.5 SPIR-V_1.4 SPIR-V_1.3 SPIR-V_1.2 SPIR-V_1.1 SPIR-V_1.0 ", sharedCaps.ilVersion);
    EXPECT_EQ(defaultHwInfo->capabilityTable.supportsIndependentForwardProgress, caps.independentForwardProgress);

    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), caps.deviceAvailable);
    EXPECT_EQ(static_cast<cl_device_mem_cache_type>(CL_READ_WRITE_CACHE), caps.globalMemCacheType);

    EXPECT_EQ(sysInfo.EUCount, caps.maxComputUnits);
    EXPECT_LT(0u, caps.maxConstantArgs);

    EXPECT_LE(128u, sharedCaps.maxReadImageArgs);
    EXPECT_LE(128u, sharedCaps.maxWriteImageArgs);
    if (defaultHwInfo->capabilityTable.supportsImages) {
        EXPECT_EQ(128u, caps.maxReadWriteImageArgs);
    } else {
        EXPECT_EQ(0u, caps.maxReadWriteImageArgs);
    }

    EXPECT_LE(sharedCaps.maxReadImageArgs * sizeof(cl_mem), sharedCaps.maxParameterSize);
    EXPECT_LE(sharedCaps.maxWriteImageArgs * sizeof(cl_mem), sharedCaps.maxParameterSize);
    EXPECT_LE(128u * MemoryConstants::megaByte, sharedCaps.maxMemAllocSize);
    const auto &compilerProductHelper = device->getRootDeviceEnvironment().getHelper<NEO::CompilerProductHelper>();

    if (!compilerProductHelper.isForceToStatelessRequired()) {
        EXPECT_GE((4 * MemoryConstants::gigaByte) - (8 * MemoryConstants::kiloByte), sharedCaps.maxMemAllocSize);
    }

    EXPECT_LE(65536u, sharedCaps.imageMaxBufferSize);

    EXPECT_GT(sharedCaps.maxWorkGroupSize, 0u);
    EXPECT_EQ(sharedCaps.maxWorkItemSizes[0], sharedCaps.maxWorkGroupSize);
    EXPECT_EQ(sharedCaps.maxWorkItemSizes[1], sharedCaps.maxWorkGroupSize);
    EXPECT_EQ(sharedCaps.maxWorkItemSizes[2], sharedCaps.maxWorkGroupSize);
    EXPECT_EQ(productHelper.getMaxNumSamplers(), sharedCaps.maxSamplers);

    // Minimum requirements for OpenCL 1.x
    EXPECT_EQ(static_cast<cl_device_fp_config>(CL_FP_ROUND_TO_NEAREST), CL_FP_ROUND_TO_NEAREST & caps.singleFpConfig);
    EXPECT_EQ(static_cast<cl_device_fp_config>(CL_FP_INF_NAN), CL_FP_INF_NAN & caps.singleFpConfig);

    cl_device_fp_config singleFpConfig = CL_FP_ROUND_TO_NEAREST |
                                         CL_FP_ROUND_TO_ZERO |
                                         CL_FP_ROUND_TO_INF |
                                         CL_FP_INF_NAN |
                                         CL_FP_FMA |
                                         CL_FP_DENORM;

    EXPECT_EQ(singleFpConfig, caps.singleFpConfig & singleFpConfig);

    EXPECT_EQ(static_cast<cl_device_exec_capabilities>(CL_EXEC_KERNEL), CL_EXEC_KERNEL & caps.executionCapabilities);
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE), CL_QUEUE_PROFILING_ENABLE & caps.queueOnHostProperties);

    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE), CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE & caps.queueOnHostProperties);
    EXPECT_LT(128u, caps.memBaseAddressAlign);
    EXPECT_LT(0u, caps.minDataTypeAlignSize);

    EXPECT_EQ(1u, caps.endianLittle);

    auto expectedDeviceSubgroups = gfxCoreHelper.getDeviceSubGroupSizes();
    EXPECT_EQ(expectedDeviceSubgroups.size(), sharedCaps.maxSubGroups.size());

    for (uint32_t i = 0; i < expectedDeviceSubgroups.size(); i++) {
        EXPECT_EQ(expectedDeviceSubgroups[i], sharedCaps.maxSubGroups[i]);
    }

    auto expectedMaxNumOfSubGroups = device->areOcl21FeaturesEnabled() ? sharedCaps.maxWorkGroupSize / gfxCoreHelper.getMinimalSIMDSize() : 0u;
    EXPECT_EQ(expectedMaxNumOfSubGroups, caps.maxNumOfSubGroups);

    EXPECT_EQ(0u, caps.maxOnDeviceEvents);
    EXPECT_EQ(0u, caps.maxOnDeviceQueues);
    EXPECT_EQ(0u, caps.queueOnDeviceMaxSize);
    EXPECT_EQ(0u, caps.queueOnDevicePreferredSize);
    EXPECT_EQ(static_cast<cl_command_queue_properties>(0), caps.queueOnDeviceProperties);

    EXPECT_EQ(0u, caps.maxPipeArgs);
    EXPECT_EQ(0u, caps.pipeMaxPacketSize);
    EXPECT_EQ(0u, caps.pipeMaxActiveReservations);

    EXPECT_EQ(64u, caps.preferredGlobalAtomicAlignment);
    EXPECT_EQ(64u, caps.preferredLocalAtomicAlignment);
    EXPECT_EQ(64u, caps.preferredPlatformAtomicAlignment);
    EXPECT_TRUE(caps.nonUniformWorkGroupSupport);

    auto expectedPreferredWorkGroupSizeMultiple = gfxCoreHelper.isFusedEuDispatchEnabled(*defaultHwInfo, false)
                                                      ? CommonConstants::maximalSimdSize * 2
                                                      : CommonConstants::maximalSimdSize;
    EXPECT_EQ(expectedPreferredWorkGroupSizeMultiple, caps.preferredWorkGroupSizeMultiple);

    EXPECT_EQ(static_cast<cl_bool>(device->getHardwareInfo().capabilityTable.supportsImages), sharedCaps.imageSupport);
    EXPECT_EQ(16384u, sharedCaps.image2DMaxWidth);
    EXPECT_EQ(16384u, sharedCaps.image2DMaxHeight);
    EXPECT_EQ(2048u, sharedCaps.imageMaxArraySize);
    if (device->getHardwareInfo().capabilityTable.supportsOcl21Features == false && is64bit) {
        EXPECT_TRUE(sharedCaps.force32BitAddresses);
    }
}

HWTEST_F(DeviceGetCapsTest, givenDeviceWhenAskingForSubGroupSizesThenReturnCorrectValues) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto &gfxCoreHelper = device->getGfxCoreHelper();

    auto deviceSubgroups = gfxCoreHelper.getDeviceSubGroupSizes();

    EXPECT_EQ(3u, deviceSubgroups.size());
    EXPECT_EQ(8u, deviceSubgroups[0]);
    EXPECT_EQ(16u, deviceSubgroups[1]);
    EXPECT_EQ(32u, deviceSubgroups[2]);
}

TEST_F(DeviceGetCapsTest, GivenPlatformWhenGettingHwInfoThenImage3dDimensionsAreCorrect) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    const auto &sharedCaps = device->getSharedDeviceInfo();

    if (device->getHardwareInfo().platform.eRenderCoreFamily > IGFX_GEN12LP_CORE) {
        EXPECT_EQ(16384u, caps.image3DMaxWidth);
        EXPECT_EQ(16384u, caps.image3DMaxHeight);
    } else {
        EXPECT_EQ(2048u, caps.image3DMaxWidth);
        EXPECT_EQ(2048u, caps.image3DMaxHeight);
    }

    EXPECT_EQ(2048u, sharedCaps.image3DMaxDepth);
}

TEST_F(DeviceGetCapsTest, givenForceOclVersion30WhenCapsAreCreatedThenDeviceReportsOpenCL30) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(30);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    EXPECT_STREQ("OpenCL 3.0 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
    EXPECT_EQ(CL_MAKE_VERSION(3u, 0u, 0u), caps.numericClVersion);
    EXPECT_FALSE(device->ocl21FeaturesEnabled);
    verifyOpenclCAllVersions(*device);
    verifyOpenclCFeatures(*device);
}

TEST_F(DeviceGetCapsTest, givenForceOclVersion21WhenCapsAreCreatedThenDeviceReportsOpenCL21) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(21);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
    EXPECT_EQ(CL_MAKE_VERSION(2u, 1u, 0u), caps.numericClVersion);
    EXPECT_TRUE(device->ocl21FeaturesEnabled);
    verifyOpenclCAllVersions(*device);
    verifyOpenclCFeatures(*device);
}

TEST_F(DeviceGetCapsTest, givenForceOclVersion12WhenCapsAreCreatedThenDeviceReportsOpenCL12) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(12);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
    EXPECT_EQ(CL_MAKE_VERSION(1u, 2u, 0u), caps.numericClVersion);
    EXPECT_FALSE(device->ocl21FeaturesEnabled);
    verifyOpenclCAllVersions(*device);
    verifyOpenclCFeatures(*device);
}

TEST_F(DeviceGetCapsTest, givenForceOCL21FeaturesSupportEnabledWhenCapsAreCreatedThenDeviceReportsSupportOfOcl21Features) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(12);
    debugManager.flags.ForceOCL21FeaturesSupport.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_TRUE(device->ocl21FeaturesEnabled);
}

TEST_F(DeviceGetCapsTest, givenForceOCL21FeaturesSupportDisabledWhenCapsAreCreatedThenDeviceReportsNoSupportOfOcl21Features) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(21);
    debugManager.flags.ForceOCL21FeaturesSupport.set(0);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_FALSE(device->ocl21FeaturesEnabled);
}

TEST_F(DeviceGetCapsTest, givenForceOcl30AndForceOCL21FeaturesSupportEnabledWhenCapsAreCreatedThenDeviceReportsSupportOfOcl21Features) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(30);
    debugManager.flags.ForceOCL21FeaturesSupport.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_TRUE(device->ocl21FeaturesEnabled);
}

TEST_F(DeviceGetCapsTest, givenForceInvalidOclVersionWhenCapsAreCreatedThenDeviceWillDefaultToOpenCL12) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(1);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
    EXPECT_EQ(CL_MAKE_VERSION(1u, 2u, 0u), caps.numericClVersion);
    EXPECT_FALSE(device->ocl21FeaturesEnabled);
    verifyOpenclCAllVersions(*device);
    verifyOpenclCFeatures(*device);
}

TEST_F(DeviceGetCapsTest, givenForce32bitAddressingWhenCapsAreCreatedThenDeviceReports32bitAddressingOptimization) {
    DebugManagerStateRestore dbgRestorer;
    {
        debugManager.flags.Force32bitAddressing.set(true);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        const auto &caps = device->getDeviceInfo();
        const auto &sharedCaps = device->getSharedDeviceInfo();
        const auto memSizePercent = device->getMemoryManager()->getPercentOfGlobalMemoryAvailable(device->getRootDeviceIndex());
        if constexpr (is64bit) {
            EXPECT_TRUE(sharedCaps.force32BitAddresses);
        } else {
            EXPECT_FALSE(sharedCaps.force32BitAddresses);
        }
        auto expectedSize = (cl_ulong)(4 * memSizePercent * MemoryConstants::gigaByte);
        EXPECT_LE(sharedCaps.globalMemSize, expectedSize);
        EXPECT_LE(sharedCaps.maxMemAllocSize, expectedSize);
        EXPECT_LE(caps.maxConstantBufferSize, expectedSize);
        EXPECT_EQ(sharedCaps.addressBits, 32u);
    }
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenGlobalMemSizeIsAlignedDownToPageSize) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &sharedCaps = device->getSharedDeviceInfo();

    auto expectedSize = alignDown(sharedCaps.globalMemSize, MemoryConstants::pageSize);

    EXPECT_EQ(sharedCaps.globalMemSize, expectedSize);
}

TEST_F(DeviceGetCapsTest, Given32bitAddressingWhenDeviceIsCreatedThenGlobalMemSizeIsAlignedDownToPageSize) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &sharedCaps = device->getSharedDeviceInfo();
    auto pMemManager = device->getMemoryManager();
    auto enabledOcl21Features = device->areOcl21FeaturesEnabled();
    bool addressing32Bit = is32bit || (is64bit && (enabledOcl21Features == false)) || debugManager.flags.Force32bitAddressing.get();
    const auto memSizePercent = pMemManager->getPercentOfGlobalMemoryAvailable(device->getRootDeviceIndex());

    cl_ulong sharedMem = (cl_ulong)pMemManager->getSystemSharedMemory(0u);
    cl_ulong maxAppAddrSpace = (cl_ulong)pMemManager->getMaxApplicationAddress() + 1ULL;
    cl_ulong memSize = std::min(sharedMem, maxAppAddrSpace);

    size_t internalResourcesSize = 0u;
    if (!pMemManager->isLocalMemorySupported(device->getRootDeviceIndex())) {
        internalResourcesSize = 450 * MemoryConstants::megaByte;
    }

    memSize = (cl_ulong)((double)memSize * memSizePercent) - internalResourcesSize;

    if (addressing32Bit) {
        memSize = std::min(memSize, (uint64_t)(4 * MemoryConstants::gigaByte * memSizePercent));
    }
    cl_ulong expectedSize = alignDown(memSize, MemoryConstants::pageSize);

    EXPECT_EQ(sharedCaps.globalMemSize, expectedSize);
}

TEST_F(DeviceGetCapsTest, givenDeviceCapsWhenLocalMemoryIsEnabledThenCalculateGlobalMemSizeBasedOnLocalMemory) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableLocalMemory.set(true);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &sharedCaps = device->getSharedDeviceInfo();
    auto pMemManager = device->getMemoryManager();
    auto enabledOcl21Features = device->areOcl21FeaturesEnabled();
    bool addressing32Bit = is32bit || (is64bit && (enabledOcl21Features == false)) || debugManager.flags.Force32bitAddressing.get();
    const auto memSizePercent = pMemManager->getPercentOfGlobalMemoryAvailable(device->getRootDeviceIndex());

    auto localMem = pMemManager->getLocalMemorySize(0u, static_cast<uint32_t>(device->getDeviceBitfield().to_ulong()));
    auto maxAppAddrSpace = pMemManager->getMaxApplicationAddress() + 1;
    auto memSize = std::min(localMem, maxAppAddrSpace);
    memSize = static_cast<cl_ulong>(memSize * memSizePercent);
    if (addressing32Bit) {
        memSize = std::min(memSize, static_cast<cl_ulong>(4 * MemoryConstants::gigaByte * memSizePercent));
    }
    cl_ulong expectedSize = alignDown(memSize, MemoryConstants::pageSize);

    EXPECT_EQ(sharedCaps.globalMemSize, expectedSize);
}

HWTEST_F(DeviceGetCapsTest, givenGlobalMemSizeAndStatelessNotSupportedWhenCalculatingMaxAllocSizeThenAdjustToHWCap) {
    DebugManagerStateRestore dbgRestorer;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto &rootDeviceEnvironment = device->getRootDeviceEnvironment();
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }

    const auto &caps = device->getSharedDeviceInfo();
    uint64_t expectedSize = std::max(caps.globalMemSize, static_cast<uint64_t>(128ULL * MemoryConstants::megaByte));

    expectedSize = std::min(ApiSpecificConfig::getReducedMaxAllocSize(expectedSize), device->getGfxCoreHelper().getMaxMemAllocSize());

    EXPECT_EQ(caps.maxMemAllocSize, expectedSize);
}

HWTEST_F(DeviceGetCapsTest, givenDebugFlagSetWhenCreatingDeviceThenOverrideMaxMemAllocSize) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.OverrideMaxMemAllocSizeMb.set(5 * 1024);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    const auto &caps = device->getSharedDeviceInfo();

    EXPECT_EQ(caps.maxMemAllocSize, 5u * 1024u * MemoryConstants::megaByte);
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenExtensionsStringEndsWithSpace) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    auto len = strlen(caps.deviceExtensions);
    ASSERT_LT(0U, len);
    EXPECT_EQ(' ', caps.deviceExtensions[len - 1]);
}

TEST_F(DeviceGetCapsTest, givenEnableSharingFormatQuerySetTrueAndDisabledMultipleSubDevicesWhenDeviceCapsAreCreatedThenSharingFormatQueryIsReported) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableFormatQuery.set(true);
    debugManager.flags.CreateMultipleSubDevices.set(0);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    auto &productHelper = device->getProductHelper();

    if (productHelper.isSharingWith3dOrMediaAllowed()) {
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_sharing_format_query ")));
    } else {
        EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_sharing_format_query ")));
    }
}

TEST_F(DeviceGetCapsTest, givenEnableSharingFormatQuerySetTrueAndEnabledMultipleSubDevicesWhenDeviceCapsAreCreatedForRootDeviceThenSharingFormatQueryIsNotReported) {
    DebugManagerStateRestore dbgRestorer;
    VariableBackup<bool> mockDeviceFlagBackup{&MockDevice::createSingleDevice, false};
    debugManager.flags.EnableFormatQuery.set(true);
    debugManager.flags.CreateMultipleSubDevices.set(2);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_sharing_format_query ")));
}

TEST_F(DeviceGetCapsTest, givenEnableSharingFormatQuerySetTrueAndEnabledMultipleSubDevicesWhenDeviceCapsAreCreatedForSubDeviceThenSharingFormatQueryIsReported) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableFormatQuery.set(true);
    debugManager.flags.CreateMultipleSubDevices.set(2);

    auto rootDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto &productHelper = rootDevice->getProductHelper();

    EXPECT_FALSE(hasSubstr(rootDevice->getDeviceInfo().deviceExtensions, std::string("cl_intel_sharing_format_query ")));

    auto subDevice0 = rootDevice->getSubDevice(0);
    auto subDevice1 = rootDevice->getSubDevice(1);

    if (productHelper.isSharingWith3dOrMediaAllowed()) {
        EXPECT_TRUE(hasSubstr(subDevice0->getDeviceInfo().deviceExtensions, std::string("cl_intel_sharing_format_query ")));
        EXPECT_TRUE(hasSubstr(subDevice1->getDeviceInfo().deviceExtensions, std::string("cl_intel_sharing_format_query ")));
    } else {
        EXPECT_FALSE(hasSubstr(subDevice0->getDeviceInfo().deviceExtensions, std::string("cl_intel_sharing_format_query ")));
        EXPECT_FALSE(hasSubstr(subDevice1->getDeviceInfo().deviceExtensions, std::string("cl_intel_sharing_format_query ")));
    }
}

TEST_F(DeviceGetCapsTest, givenOpenCLVersion21WhenCapsAreCreatedThenDeviceReportsClIntelSpirvExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(21);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    const HardwareInfo *hwInfo = defaultHwInfo.get();
    {
        if (hwInfo->capabilityTable.supportsImages) {
            EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string(std::string("cl_khr_3d_image_writes"))));
        } else {
            EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_3d_image_writes")));
        }
        if (hwInfo->capabilityTable.supportsMediaBlock) {
            EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_spirv_media_block_io")));
        } else {
            EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_spirv_media_block_io")));
        }

        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_spirv_subgroups")));
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_spirv_linkonce_odr")));
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_spirv_no_integer_wrap_decoration")));
    }
}

TEST_F(DeviceGetCapsTest, givenSupportMediaBlockWhenCapsAreCreatedThenDeviceReportsClIntelSpirvMediaBlockIoExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(21);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsMediaBlock = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_spirv_media_block_io")));
}

TEST_F(DeviceGetCapsTest, givenNotMediaBlockWhenCapsAreCreatedThenDeviceNotReportsClIntelSpirvMediaBlockIoExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(21);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsMediaBlock = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_spirv_media_block_io")));
}

TEST_F(DeviceGetCapsTest, givenSupportImagesWhenCapsAreCreatedThenDeviceReportsClKhr3dImageWritesExtensions) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_3d_image_writes")));
}

TEST_F(DeviceGetCapsTest, givenNotSupportImagesWhenCapsAreCreatedThenDeviceNotReportsClKhr3dImageWritesExtensions) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_3d_image_writes")));
}

TEST_F(DeviceGetCapsTest, givenOpenCLVersion12WhenCapsAreCreatedThenDeviceDoesntReportClIntelSpirvExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(12);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_spirv_subgroups")));
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_spirv_linkonce_odr")));
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_spirv_no_integer_wrap_decoration")));
}

TEST_F(DeviceGetCapsTest, givenEnableNV12setToTrueAndSupportImagesWhenCapsAreCreatedThenDeviceReportsNV12Extension) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableNV12.set(true);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    if (device->getHardwareInfo().capabilityTable.supportsImages) {
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_planar_yuv")));
        EXPECT_TRUE(caps.nv12Extension);
    } else {
        EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_planar_yuv")));
    }
}

TEST_F(DeviceGetCapsTest, givenEnablePackedYuvsetToTrueAndSupportImagesWhenCapsAreCreatedThenDeviceReportsPackedYuvExtension) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnablePackedYuv.set(true);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    if (device->getHardwareInfo().capabilityTable.supportsImages) {
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_packed_yuv")));
        EXPECT_TRUE(caps.packedYuvExtension);
    } else {
        EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_packed_yuv")));
    }
}

TEST_F(DeviceGetCapsTest, givenSupportImagesWhenCapsAreCreatedThenDeviceReportsPackedYuvAndNV12Extensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnablePackedYuv.set(true);
    debugManager.flags.EnableNV12.set(true);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_packed_yuv")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_planar_yuv")));
}

TEST_F(DeviceGetCapsTest, givenNotSupportImagesWhenCapsAreCreatedThenDeviceNotReportsPackedYuvAndNV12Extensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnablePackedYuv.set(true);
    debugManager.flags.EnableNV12.set(true);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_packed_yuv")));
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_planar_yuv")));
}

TEST_F(DeviceGetCapsTest, givenEnableNV12setToFalseWhenCapsAreCreatedThenDeviceDoesNotReportNV12Extension) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableNV12.set(false);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_planar_yuv")));
    EXPECT_FALSE(caps.nv12Extension);
}

TEST_F(DeviceGetCapsTest, givenEnablePackedYuvsetToFalseWhenCapsAreCreatedThenDeviceDoesNotReportPackedYuvExtension) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnablePackedYuv.set(false);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_packed_yuv")));
    EXPECT_FALSE(caps.packedYuvExtension);
}

TEST_F(DeviceGetCapsTest, WhenCheckingFp64ThenResultIsConsistentWithHardwareCapabilities) {
    auto hwInfo = *defaultHwInfo;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    if (hwInfo.capabilityTable.ftrSupportsInteger64BitAtomics) {
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, "cl_khr_int64_base_atomics "));
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, "cl_khr_int64_extended_atomics "));
    } else {
        EXPECT_FALSE(hasSubstr(caps.deviceExtensions, "cl_khr_int64_base_atomics "));
        EXPECT_FALSE(hasSubstr(caps.deviceExtensions, "cl_khr_int64_extended_atomics "));
    }
}

TEST_F(DeviceGetCapsTest, WhenDeviceDoesNotSupportOcl21FeaturesThenDeviceEnqueueAndPipeAreNotSupported) {
    UltClDeviceFactory deviceFactory{1, 0};
    if (deviceFactory.rootDevices[0]->areOcl21FeaturesEnabled() == false) {
        EXPECT_FALSE(deviceFactory.rootDevices[0]->getDeviceInfo().deviceEnqueueSupport);
        EXPECT_FALSE(deviceFactory.rootDevices[0]->getDeviceInfo().pipeSupport);
    }
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenPriorityHintsExtensionIsReported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_priority_hints")));
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenCreateCommandQueueExtensionIsReported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_create_command_queue")));
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenExpectAssumeExtensionIsReported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_expect_assume")));
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenExtendedBitOpsExtensionIsReported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_extended_bit_ops")));
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenThrottleHintsExtensionIsReported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_throttle_hints")));
}

TEST_F(DeviceGetCapsTest, GivenAnyDeviceWhenCheckingExtensionsThenSupportSubgroupsChar) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroups_char")));
}

TEST_F(DeviceGetCapsTest, GivenAnyDeviceWhenCheckingExtensionsThenSupportSubgroupsLong) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroups_long")));
}

TEST_F(DeviceGetCapsTest, GivenAnyDeviceWhenCheckingExtensionsThenSupportForceHostMemory) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_mem_force_host_memory")));
}

TEST_F(DeviceGetCapsTest, givenAtleastOCL21DeviceThenExposesMipMapAndUnifiedMemoryExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(21);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;
    if (device->getHardwareInfo().capabilityTable.supportsImages) {
        EXPECT_TRUE(hasSubstr(extensionString, std::string("cl_khr_mipmap_image")));
        EXPECT_TRUE(hasSubstr(extensionString, std::string("cl_khr_mipmap_image_writes")));
    } else {
        EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_mipmap_image")));
        EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_mipmap_image_writes")));
    }
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_unified_shared_memory")));
}

TEST_F(DeviceGetCapsTest, givenSupportImagesWhenCapsAreCreatedThenDeviceReportsMinMapExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(21);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_mipmap_image")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_mipmap_image_writes")));
}

TEST_F(DeviceGetCapsTest, givenNotSupportImagesWhenCapsAreCreatedThenDeviceNotReportsMinMapExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(20);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_mipmap_image")));
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_mipmap_image_writes")));
}

TEST_F(DeviceGetCapsTest, givenOCL12DeviceThenDoesNotExposesMipMapAndUnifiedMemoryExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(12);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();

    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_mipmap_image")));
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_mipmap_image_writes")));
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_unified_shared_memory")));
}

TEST_F(DeviceGetCapsTest, givenSupportImagesWhenCreateExtentionsListThenDeviceReportsImagesExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(20);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto extensions = device->getDeviceInfo().deviceExtensions;

    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_khr_image2d_from_buffer")));
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_khr_depth_images")));
}

TEST_F(DeviceGetCapsTest, givenNotSupporteImagesWhenCreateExtentionsListThenDeviceNotReportsImagesExtensions) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceOCLVersion.set(20);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto extensions = device->getDeviceInfo().deviceExtensions;

    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_khr_image2d_from_buffer")));
    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_khr_depth_images")));
}

TEST_F(DeviceGetCapsTest, givenDeviceWhenGettingHostUnifiedMemoryCapThenItDependsOnLocalMemory) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getDeviceInfo();
    auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    auto localMemoryEnabled = gfxCoreHelper.isLocalMemoryEnabled(*defaultHwInfo);

    EXPECT_EQ((localMemoryEnabled == false), caps.hostUnifiedMemory);
}

TEST_F(DeviceGetCapsTest, givenDefaultDeviceWhenQueriedForExtensionsWithVersionThenValuesMatchWithExtensionsString) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    std::string allExtensions;

    EXPECT_TRUE(pClDevice->getDeviceInfo().extensionsWithVersion.empty());
    pClDevice->getDeviceInfo(CL_DEVICE_EXTENSIONS_WITH_VERSION, 0, nullptr, nullptr);
    EXPECT_FALSE(pClDevice->getDeviceInfo().extensionsWithVersion.empty());

    for (auto extensionWithVersion : pClDevice->getDeviceInfo().extensionsWithVersion) {
        if (strcmp(extensionWithVersion.name, "cl_khr_integer_dot_product") == 0) {
            EXPECT_EQ(CL_MAKE_VERSION(2u, 0, 0), extensionWithVersion.version);
        } else if (strcmp(extensionWithVersion.name, "cl_intel_unified_shared_memory") == 0) {
            EXPECT_EQ(CL_MAKE_VERSION(1u, 1u, 0), extensionWithVersion.version);
        } else if (strcmp(extensionWithVersion.name, "cl_khr_external_memory") == 0) {
            EXPECT_EQ(CL_MAKE_VERSION(0, 9u, 1u), extensionWithVersion.version);
        } else {
            EXPECT_EQ(CL_MAKE_VERSION(1u, 0, 0), extensionWithVersion.version);
        }
        allExtensions += extensionWithVersion.name;
        allExtensions += " ";
    }

    EXPECT_STREQ(pClDevice->deviceExtensions.c_str(), allExtensions.c_str());
}

TEST_F(DeviceGetCapsTest, givenClDeviceWhenGetExtensionsVersionCalledThenCorrectVersionIsSet) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    pClDevice->getDeviceInfo(CL_DEVICE_EXTENSIONS_WITH_VERSION, 0, nullptr, nullptr);
    for (auto extensionWithVersion : pClDevice->getDeviceInfo().extensionsWithVersion) {
        if (strcmp(extensionWithVersion.name, "cl_khr_integer_dot_product") == 0) {
            EXPECT_EQ(CL_MAKE_VERSION(2u, 0, 0), pClDevice->getExtensionVersion(std::string(extensionWithVersion.name)));
        } else if (strcmp(extensionWithVersion.name, "cl_intel_unified_shared_memory") == 0) {
            EXPECT_EQ(CL_MAKE_VERSION(1u, 1u, 0), pClDevice->getExtensionVersion(std::string(extensionWithVersion.name)));
        } else if (strcmp(extensionWithVersion.name, "cl_khr_external_memory") == 0) {
            EXPECT_EQ(CL_MAKE_VERSION(0, 9u, 1u), pClDevice->getExtensionVersion(std::string(extensionWithVersion.name)));
        } else {
            EXPECT_EQ(CL_MAKE_VERSION(1u, 0, 0), pClDevice->getExtensionVersion(std::string(extensionWithVersion.name)));
        }
    }
}

TEST_F(DeviceGetCapsTest, givenClDeviceWhenCapsInitializedThenIntegerDotInput4xBitCapIsSet) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    pClDevice->initializeCaps();
    auto &compilerHelper = pClDevice->getDevice().getCompilerProductHelper();
    EXPECT_EQ((pClDevice->deviceInfo.integerDotCapabilities & CL_DEVICE_INTEGER_DOT_PRODUCT_INPUT_4x8BIT_KHR) != 0, compilerHelper.isDotIntegerProductExtensionSupported());
}

TEST_F(DeviceGetCapsTest, givenClDeviceWhenCapsInitializedThenIntegerDotInput4xBitPackedCapIsSet) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    pClDevice->initializeCaps();
    auto &compilerHelper = pClDevice->getDevice().getCompilerProductHelper();
    EXPECT_EQ((pClDevice->deviceInfo.integerDotCapabilities & CL_DEVICE_INTEGER_DOT_PRODUCT_INPUT_4x8BIT_PACKED_KHR) != 0, compilerHelper.isDotIntegerProductExtensionSupported());
}

TEST_F(DeviceGetCapsTest, givenClDeviceWhenCapsInitializedThenAllFieldsInIntegerDotAccPropertiesAreTrue) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    pClDevice->initializeCaps();
    auto &compilerHelper = pClDevice->getDevice().getCompilerProductHelper();
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties8Bit.accumulating_saturating_mixed_signedness_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties8Bit.accumulating_saturating_signed_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties8Bit.accumulating_saturating_unsigned_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties8Bit.mixed_signedness_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties8Bit.signed_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties8Bit.unsigned_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
}

TEST_F(DeviceGetCapsTest, givenClDeviceWhenCapsInitializedThenAllFieldsInIntegerDotAccPackedPropertiesAreTrue) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    pClDevice->initializeCaps();
    auto &compilerHelper = pClDevice->getDevice().getCompilerProductHelper();
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties4x8BitPacked.accumulating_saturating_mixed_signedness_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties4x8BitPacked.accumulating_saturating_signed_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties4x8BitPacked.accumulating_saturating_unsigned_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties4x8BitPacked.mixed_signedness_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties4x8BitPacked.signed_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
    EXPECT_EQ(pClDevice->deviceInfo.integerDotAccelerationProperties4x8BitPacked.unsigned_accelerated, compilerHelper.isDotIntegerProductExtensionSupported());
}

TEST_F(DeviceGetCapsTest, givenClDeviceWhenEnableIntegerDotExtensionEnalbedThenDotIntegerExtensionIsInExtensionString) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    pClDevice->initializeCaps();
    auto &compilerHelper = pClDevice->getDevice().getCompilerProductHelper();
    static const char *const supportedExtensions[] = {
        "cl_khr_integer_dot_product "};
    for (auto extension : supportedExtensions) {
        auto foundOffset = pClDevice->deviceExtensions.find(extension);
        EXPECT_EQ(foundOffset != std::string::npos, compilerHelper.isDotIntegerProductExtensionSupported());
    }
}

TEST_F(DeviceGetCapsTest, givenFp64SupportForcedWhenCheckingFp64SupportThenFp64IsCorrectlyReported) {
    DebugManagerStateRestore dbgRestorer;
    int32_t overrideDefaultFP64SettingsValues[] = {-1, 0, 1};
    auto hwInfo = *defaultHwInfo;

    for (auto isFp64SupportedByHw : ::testing::Bool()) {
        for (auto isInteger64BitAtomicsSupportedByHw : ::testing::Bool()) {
            for (auto isFloatAtomicsSupportedByHw : ::testing::Bool()) {
                hwInfo.capabilityTable.ftrSupportsInteger64BitAtomics = isInteger64BitAtomicsSupportedByHw;
                hwInfo.capabilityTable.ftrSupportsFP64 = isFp64SupportedByHw;
                hwInfo.capabilityTable.ftrSupports64BitMath = isFp64SupportedByHw;
                hwInfo.capabilityTable.supportsFloatAtomics = isFloatAtomicsSupportedByHw;

                for (auto overrideDefaultFP64Settings : overrideDefaultFP64SettingsValues) {
                    debugManager.flags.OverrideDefaultFP64Settings.set(overrideDefaultFP64Settings);
                    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
                    auto &caps = pClDevice->getDeviceInfo();
                    std::string extensionString = pClDevice->getDeviceInfo().deviceExtensions;

                    size_t fp64FeaturesCount = 0;
                    for (auto &openclCFeature : caps.openclCFeatures) {
                        if (0 == strcmp(openclCFeature.name, "__opencl_c_fp64")) {
                            fp64FeaturesCount++;
                        }
                        if (0 == strcmp(openclCFeature.name, "__opencl_c_ext_fp64_global_atomic_add")) {
                            fp64FeaturesCount++;
                        }
                        if (0 == strcmp(openclCFeature.name, "__opencl_c_ext_fp64_local_atomic_add")) {
                            fp64FeaturesCount++;
                        }
                        if (0 == strcmp(openclCFeature.name, "__opencl_c_ext_fp64_global_atomic_min_max")) {
                            fp64FeaturesCount++;
                        }
                        if (0 == strcmp(openclCFeature.name, "__opencl_c_ext_fp64_local_atomic_min_max")) {
                            fp64FeaturesCount++;
                        }
                    }

                    bool expectedFp64Support = ((overrideDefaultFP64Settings == -1) ? isFp64SupportedByHw : overrideDefaultFP64Settings);
                    if (expectedFp64Support) {
                        const size_t expectedFp64FeaturesCount = hwInfo.capabilityTable.supportsOcl21Features && isFloatAtomicsSupportedByHw ? 5u : 1u;
                        EXPECT_NE(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
                        EXPECT_NE(0u, caps.doubleFpConfig);
                        if (hwInfo.capabilityTable.supportsOcl21Features && isFloatAtomicsSupportedByHw) {
                            const cl_device_fp_atomic_capabilities_ext expectedFpCaps = static_cast<cl_device_fp_atomic_capabilities_ext>(CL_DEVICE_GLOBAL_FP_ATOMIC_LOAD_STORE_EXT | CL_DEVICE_GLOBAL_FP_ATOMIC_ADD_EXT | CL_DEVICE_GLOBAL_FP_ATOMIC_MIN_MAX_EXT |
                                                                                                                                          CL_DEVICE_LOCAL_FP_ATOMIC_LOAD_STORE_EXT | CL_DEVICE_LOCAL_FP_ATOMIC_ADD_EXT | CL_DEVICE_LOCAL_FP_ATOMIC_MIN_MAX_EXT);
                            EXPECT_EQ(expectedFpCaps, caps.doubleFpAtomicCapabilities);
                        } else if (isFloatAtomicsSupportedByHw || isInteger64BitAtomicsSupportedByHw) {
                            const cl_device_fp_atomic_capabilities_ext expectedFpCaps = static_cast<cl_device_fp_atomic_capabilities_ext>(CL_DEVICE_GLOBAL_FP_ATOMIC_LOAD_STORE_EXT | CL_DEVICE_LOCAL_FP_ATOMIC_LOAD_STORE_EXT);
                            EXPECT_EQ(expectedFpCaps, caps.doubleFpAtomicCapabilities);
                        } else {
                            EXPECT_EQ(0u, caps.doubleFpAtomicCapabilities);
                        }
                        EXPECT_EQ(expectedFp64FeaturesCount, fp64FeaturesCount);
                        EXPECT_NE(0u, caps.nativeVectorWidthDouble);
                        EXPECT_NE(0u, caps.preferredVectorWidthDouble);
                        EXPECT_TRUE(isValueSet(caps.singleFpConfig, CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT));
                    } else {
                        EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
                        EXPECT_EQ(0u, caps.doubleFpConfig);
                        EXPECT_EQ(0u, caps.doubleFpAtomicCapabilities);
                        EXPECT_EQ(0u, fp64FeaturesCount);
                        EXPECT_EQ(0u, caps.nativeVectorWidthDouble);
                        EXPECT_EQ(0u, caps.preferredVectorWidthDouble);
                        EXPECT_FALSE(isValueSet(caps.singleFpConfig, CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT));
                    }
                }
            }
        }
    }
}

TEST_F(DeviceGetCapsTest, givenFp64EmulationSupportWithoutFp64EmulationEnvVarWhenCreatingDeviceThenDeviceCapsAreSetCorrectly) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.ftrSupportsFP64 = false;
    hwInfo.capabilityTable.ftrSupportsFP64Emulation = true;

    auto executionEnvironment = MockClDevice::prepareExecutionEnvironment(&hwInfo, 0);
    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));

    auto &caps = pClDevice->getDeviceInfo();
    std::string extensionString = pClDevice->getDeviceInfo().deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_FALSE(isValueSet(caps.doubleFpConfig, CL_FP_SOFT_FLOAT));
}

TEST_F(DeviceGetCapsTest, givenFp64EmulationSupportWithFp64EmulationEnvVarSetWhenCreatingDeviceThenDeviceCapsAreSetCorrectly) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.ftrSupportsFP64 = false;
    hwInfo.capabilityTable.ftrSupportsFP64Emulation = true;

    auto executionEnvironment = MockClDevice::prepareExecutionEnvironment(&hwInfo, 0);
    executionEnvironment->setFP64EmulationEnabled();
    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));

    auto &caps = pClDevice->getDeviceInfo();
    std::string extensionString = pClDevice->getDeviceInfo().deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_TRUE(isValueSet(caps.doubleFpConfig, CL_FP_SOFT_FLOAT));

    cl_device_fp_config defaultFpFlags = static_cast<cl_device_fp_config>(CL_FP_ROUND_TO_NEAREST |
                                                                          CL_FP_ROUND_TO_ZERO |
                                                                          CL_FP_ROUND_TO_INF |
                                                                          CL_FP_INF_NAN |
                                                                          CL_FP_DENORM |
                                                                          CL_FP_FMA);
    EXPECT_EQ(defaultFpFlags, caps.doubleFpConfig & defaultFpFlags);
    EXPECT_EQ(1u, caps.nativeVectorWidthDouble);
    EXPECT_EQ(1u, caps.preferredVectorWidthDouble);
}

TEST(DeviceGetCaps, WhenPeekingCompilerExtensionsThenCompilerExtensionsAreReturned) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    EXPECT_EQ(&pClDevice->compilerExtensions, &pClDevice->peekCompilerExtensions());
}

TEST(DeviceGetCaps, WhenCheckingCompilerExtensionsThenValueIsCorrect) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    OpenClCFeaturesContainer emptyOpenClCFeatures;
    auto expectedCompilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(pClDevice->deviceInfo.deviceExtensions,
                                                                                        emptyOpenClCFeatures);
    EXPECT_STREQ(expectedCompilerExtensions.c_str(), pClDevice->compilerExtensions.c_str());
}

TEST(DeviceGetCaps, WhenPeekingCompilerExtensionsWithFeaturesThenCompilerExtensionsWithFeaturesAreReturned) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    EXPECT_EQ(&pClDevice->compilerExtensionsWithFeatures, &pClDevice->peekCompilerExtensionsWithFeatures());
}

TEST(DeviceGetCaps, WhenCheckingCompilerExtensionsWithFeaturesThenValueIsCorrect) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    auto expectedCompilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(pClDevice->deviceInfo.deviceExtensions,
                                                                                        pClDevice->deviceInfo.openclCFeatures);
    EXPECT_STREQ(expectedCompilerExtensions.c_str(), pClDevice->compilerExtensionsWithFeatures.c_str());
}

TEST(DeviceGetCaps, WhenComparingCompilerExtensionsAndCompilerExtensionsWithFeaturesThenValuesMatch) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto pClDevice = deviceFactory.rootDevices[0];
    auto compilerExtensions = pClDevice->compilerExtensions;
    auto compilerExtensionsWithFeatures = pClDevice->compilerExtensionsWithFeatures;

    compilerExtensions.erase(compilerExtensions.size() - 1);
    EXPECT_STREQ(compilerExtensions.c_str(), compilerExtensionsWithFeatures.substr(0, compilerExtensions.size()).c_str());
}

HWTEST_F(DeviceGetCapsTest, givenDisabledFtrPooledEuWhenCalculatingMaxEuPerSSThenIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.flags.ftrPooledEuEnabled = 0;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));
    auto &deviceInfo = device->deviceInfo;

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto simdSizeUsed = gfxCoreHelper.getMinimalSIMDSize();

    auto &productHelper = device->getProductHelper();
    auto expectedMaxWGS = productHelper.getMaxThreadsForWorkgroupInDSSOrSS(myHwInfo, static_cast<uint32_t>(deviceInfo.maxNumEUsPerSubSlice),
                                                                           static_cast<uint32_t>(deviceInfo.maxNumEUsPerDualSubSlice)) *
                          simdSizeUsed;

    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 1024u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

HWTEST_F(DeviceGetCapsTest, givenEnabledFtrPooledEuWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.flags.ftrPooledEuEnabled = 1;

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    auto expectedMaxWGS = mySysInfo.EuCountPerPoolMin * (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 1024u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

TEST(DeviceGetCaps, givenDebugFlagToUseMaxSimdSizeForWkgCalculationWhenDeviceCapsAreCreatedThenNumSubGroupsIsCalculatedBasedOnMaxWorkGroupSize) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.set(true);

    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;

    mySysInfo.EUCount = 24;
    mySysInfo.SubSliceCount = 3;
    mySysInfo.ThreadCount = 24 * 7;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(device->getSharedDeviceInfo().maxWorkGroupSize / CommonConstants::maximalSimdSize, device->getDeviceInfo().maxNumOfSubGroups);
}

HWTEST_F(DeviceGetCapsTest, givenDeviceThatHasHighNumberOfExecutionUnitsWhenMaxWorkgroupSizeIsComputedThenItIsLimitedTo1024) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);
    HardwareInfo myHwInfo = *defaultHwInfo;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    mySysInfo.EUCount = 32;
    mySysInfo.SubSliceCount = 2;
    mySysInfo.ThreadCount = 32 * gfxCoreHelper.getMinimalSIMDSize(); // 128 threads per subslice, in simd 8 gives 1024

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(1024u, device->getSharedDeviceInfo().maxWorkGroupSize);
    EXPECT_EQ(device->getSharedDeviceInfo().maxWorkGroupSize / gfxCoreHelper.getMinimalSIMDSize(), device->getDeviceInfo().maxNumOfSubGroups);
}

TEST_F(DeviceGetCapsTest, givenSystemWithDriverInfoWhenGettingNameAndVersionThenReturnValuesFromDriverInfo) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    const std::string testDeviceName = "testDeviceName";
    const std::string testVersion = "testVersion";

    DriverInfoMock *driverInfoMock = new DriverInfoMock();
    driverInfoMock->setDeviceName(testDeviceName);
    driverInfoMock->setVersion(testVersion);

    device->driverInfo.reset(driverInfoMock);
    device->initializeCaps();

    const auto &caps = device->getDeviceInfo();

    EXPECT_STREQ(testDeviceName.c_str(), caps.name);
    EXPECT_STREQ(testVersion.c_str(), caps.driverVersion);
}

TEST_F(DeviceGetCapsTest, givenSystemWithDriverInfoWhenDebugVariableOverrideDeviceNameIsSpecifiedThenDeviceNameIsTakenFromDebugVariable) {
    DebugManagerStateRestore restore;
    const std::string testDeviceName = "testDeviceName";
    const std::string debugDeviceName = "debugDeviceName";
    debugManager.flags.OverrideDeviceName.set(debugDeviceName);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    DriverInfoMock *driverInfoMock = new DriverInfoMock();
    driverInfoMock->setDeviceName(testDeviceName);

    device->driverInfo.reset(driverInfoMock);
    device->initializeCaps();

    const auto &caps = device->getDeviceInfo();

    EXPECT_STRNE(testDeviceName.c_str(), caps.name);
    EXPECT_STREQ(debugDeviceName.c_str(), caps.name);
}

TEST_F(DeviceGetCapsTest, givenNoPciBusInfoThenPciBusInfoExtensionNotAvailable) {
    const PhysicalDevicePciBusInfo pciBusInfo(PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue);

    DriverInfoMock *driverInfoMock = new DriverInfoMock();
    driverInfoMock->setPciBusInfo(pciBusInfo);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->driverInfo.reset(driverInfoMock);
    device->initializeCaps();

    const auto &caps = device->getDeviceInfo();
    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, "cl_khr_pci_bus_info"));
}

TEST_F(DeviceGetCapsTest, givenPciBusInfoThenPciBusInfoExtensionAvailable) {
    const PhysicalDevicePciBusInfo pciBusInfo(1, 2, 3, 4);

    DriverInfoMock *driverInfoMock = new DriverInfoMock();
    driverInfoMock->setPciBusInfo(pciBusInfo);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->driverInfo.reset(driverInfoMock);
    device->initializeCaps();

    const auto &caps = device->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, "cl_khr_pci_bus_info"));

    EXPECT_EQ(caps.pciBusInfo.pci_domain, pciBusInfo.pciDomain);
    EXPECT_EQ(caps.pciBusInfo.pci_bus, pciBusInfo.pciBus);
    EXPECT_EQ(caps.pciBusInfo.pci_device, pciBusInfo.pciDevice);
    EXPECT_EQ(caps.pciBusInfo.pci_function, pciBusInfo.pciFunction);
}

static bool getPlanarYuvHeightCalled = false;

template <typename GfxFamily>
class MyMockGfxCoreHelper : public GfxCoreHelperHw<GfxFamily> {
  public:
    uint32_t getPlanarYuvMaxHeight() const override {
        getPlanarYuvHeightCalled = true;
        return dummyPlanarYuvValue;
    }
    uint32_t dummyPlanarYuvValue = 0x12345;
};

HWTEST_F(DeviceGetCapsTest, givenDeviceWhenInitializingCapsThenPlanarYuvHeightIsTakenFromHelper) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    RAIIGfxCoreHelperFactory<MyMockGfxCoreHelper<FamilyType>> gfxCoreHelperBackup{*device->executionEnvironment->rootDeviceEnvironments[0]};

    DriverInfoMock *driverInfoMock = new DriverInfoMock();
    device->driverInfo.reset(driverInfoMock);
    device->initializeCaps();
    EXPECT_TRUE(getPlanarYuvHeightCalled);
    getPlanarYuvHeightCalled = false;
    const auto &caps = device->getDeviceInfo();
    EXPECT_EQ(gfxCoreHelperBackup.mockGfxCoreHelper->dummyPlanarYuvValue, caps.planarYuvMaxHeight);
}

TEST_F(DeviceGetCapsTest, givenSystemWithNoDriverInfoWhenGettingNameAndVersionThenReturnDefaultValues) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    device->driverInfo.reset();
    device->name.clear();
    device->initializeCaps();

    const auto &caps = device->getDeviceInfo();

    std::string tempName = device->getClDeviceName();

#define QTR(a) #a
#define TOSTR(b) QTR(b)
    const std::string expectedVersion = TOSTR(NEO_OCL_DRIVER_VERSION);
#undef QTR
#undef TOSTR

    EXPECT_STREQ(tempName.c_str(), caps.name);
    EXPECT_STREQ(expectedVersion.c_str(), caps.driverVersion);
}

TEST_F(DeviceGetCapsTest, givenFlagEnabled64kbPagesWhenCallConstructorOsAgnosticMemoryManagerThenReturnCorrectValue) {
    DebugManagerStateRestore dbgRestore;

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    std::unique_ptr<MemoryManager> memoryManager;

    debugManager.flags.Enable64kbpages.set(0); // force false
    memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    debugManager.flags.Enable64kbpages.set(1); // force true
    memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_TRUE(memoryManager->peek64kbPagesEnabled(0u));
}

TEST_F(DeviceGetCapsTest, whenDeviceIsCreatedThenMaxParameterSizeIsSetCorrectly) {

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    const auto &caps = device->getSharedDeviceInfo();
    EXPECT_EQ(2048u, caps.maxParameterSize);
}

TEST_F(DeviceGetCapsTest, givenUnifiedMemorySharedSystemFlagWhenDeviceIsCreatedThenSystemMemoryIsSetCorrectly) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(-1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0;
    EXPECT_FALSE(device->areSharedSystemAllocationsAllowed());

    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())});
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
    EXPECT_FALSE(device->areSharedSystemAllocationsAllowed());

    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())});
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
    EXPECT_TRUE(device->areSharedSystemAllocationsAllowed());
}

TEST_F(DeviceGetCapsTest, givenCapsDeviceEnqueueWhenCheckingDeviceEnqueueSupportThenNoSupportReported) {
    auto hwInfo = *defaultHwInfo;

    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    auto &caps = pClDevice->getDeviceInfo();

    EXPECT_EQ(0u, caps.maxOnDeviceEvents);
    EXPECT_EQ(0u, caps.maxOnDeviceQueues);
    EXPECT_EQ(0u, caps.queueOnDeviceMaxSize);
    EXPECT_EQ(0u, caps.queueOnDevicePreferredSize);
    EXPECT_EQ(static_cast<cl_command_queue_properties>(0), caps.queueOnDeviceProperties);
}

TEST_F(DeviceGetCapsTest, whenCheckingPipeSupportThenNoSupportIsReported) {
    auto hwInfo = *defaultHwInfo;

    auto pClDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    auto &caps = pClDevice->getDeviceInfo();

    EXPECT_EQ(0u, caps.maxPipeArgs);
    EXPECT_EQ(0u, caps.pipeMaxPacketSize);
    EXPECT_EQ(0u, caps.pipeMaxActiveReservations);
}

TEST_F(DeviceGetCapsTest, givenClDeviceWhenInitializingCapsThenUseGetQueueFamilyCapabilitiesMethod) {
    struct ClDeviceWithCustomQueueCaps : MockClDevice {
        using MockClDevice::MockClDevice;
        cl_command_queue_capabilities_intel queueCaps{};
        cl_command_queue_capabilities_intel getQueueFamilyCapabilities(EngineGroupType type) override {
            return queueCaps;
        }
    };

    auto device = std::make_unique<ClDeviceWithCustomQueueCaps>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    device->deviceInfo = {};
    device->queueCaps = CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL;
    device->initializeCaps();
    EXPECT_EQ(device->queueCaps, device->getDeviceInfo().queueFamilyProperties[0].capabilities);

    device->deviceInfo = {};
    device->queueCaps = CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL | CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL;
    device->initializeCaps();
    EXPECT_EQ(device->queueCaps, device->getDeviceInfo().queueFamilyProperties[0].capabilities);
}

HWTEST_F(QueueFamilyNameTest, givenCcsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::compute, "ccs");
}

HWTEST_F(QueueFamilyNameTest, givenRcsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::renderCompute, "rcs");
}

HWTEST_F(QueueFamilyNameTest, givenBcsWhenGettingQueueFamilyNameThenReturnProperValue) {
    verify(EngineGroupType::copy, "bcs");
}

HWTEST_F(QueueFamilyNameTest, givenInvalidEngineGroupWhenGettingQueueFamilyNameThenReturnEmptyName) {
    verify(EngineGroupType::maxEngineGroups, "");
}

template <typename FamilyType>
struct MyMockClGfxCoreHelper : NEO::ClGfxCoreHelperHw<FamilyType> {
    bool getQueueFamilyName(std::string &name, EngineGroupType type) const override {
        name = familyNameOverride;
        return true;
    }
    std::string familyNameOverride = "";
};

HWTEST_F(QueueFamilyNameTest, givenTooBigQueueFamilyNameWhenGettingQueueFamilyNameThenExceptionIsThrown) {

    MyMockClGfxCoreHelper<FamilyType> mockClGfxCoreHelper{};
    std::unique_ptr<ApiGfxCoreHelper> clGfxCoreHelper(static_cast<ApiGfxCoreHelper *>(&mockClGfxCoreHelper));
    device->executionEnvironment->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(clGfxCoreHelper);

    char name[CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL] = "";

    mockClGfxCoreHelper.familyNameOverride = std::string(CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL - 1, 'a');
    device->getQueueFamilyName(name, EngineGroupType::maxEngineGroups);
    EXPECT_EQ(0, std::strcmp(name, mockClGfxCoreHelper.familyNameOverride.c_str()));

    mockClGfxCoreHelper.familyNameOverride = std::string(CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL, 'a');
    EXPECT_ANY_THROW(device->getQueueFamilyName(name, EngineGroupType::maxEngineGroups));

    device->executionEnvironment->rootDeviceEnvironments[0]->apiGfxCoreHelper.swap(clGfxCoreHelper);
    clGfxCoreHelper.release();
}

HWTEST_F(DeviceGetCapsTest, givenSysInfoWhenDeviceCreatedThenMaxWorkGroupSizeIsCalculatedCorrectly) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    PLATFORM &myPlatform = myHwInfo.platform;

    mySysInfo.EUCount = 16;
    mySysInfo.SubSliceCount = 4;
    mySysInfo.DualSubSliceCount = 2;
    mySysInfo.ThreadCount = 16 * 8;
    myPlatform.usRevId = 0x4;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto minSimd = gfxCoreHelper.getMinimalSIMDSize();

    uint32_t expectedWGSize = (mySysInfo.ThreadCount / mySysInfo.DualSubSliceCount) * minSimd;
    expectedWGSize = gfxCoreHelper.overrideMaxWorkGroupSize(expectedWGSize);
    EXPECT_EQ(expectedWGSize, device->sharedDeviceInfo.maxWorkGroupSize);
}

HWTEST_F(DeviceGetCapsTest, givenDSSDifferentThanZeroWhenDeviceCreatedThenDualSubSliceCountIsDifferentThanSubSliceCount) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    PLATFORM &myPlatform = myHwInfo.platform;

    mySysInfo.EUCount = 16;
    mySysInfo.SubSliceCount = 4;
    mySysInfo.DualSubSliceCount = 2;
    mySysInfo.ThreadCount = 16 * 8;
    myPlatform.usRevId = 0x4;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_NE(device->sharedDeviceInfo.maxNumEUsPerSubSlice, device->sharedDeviceInfo.maxNumEUsPerDualSubSlice);
}

HWTEST_F(DeviceGetCapsTest, givenDSSCountEqualZeroWhenDeviceCreatedThenMaxEuPerDSSEqualMaxEuPerSS) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    PLATFORM &myPlatform = myHwInfo.platform;

    mySysInfo.EUCount = 16;
    mySysInfo.SubSliceCount = 4;
    mySysInfo.DualSubSliceCount = 0;
    mySysInfo.ThreadCount = 16 * 8;
    myPlatform.usRevId = 0x4;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(device->sharedDeviceInfo.maxNumEUsPerSubSlice, device->sharedDeviceInfo.maxNumEUsPerDualSubSlice);
}

TEST_F(DeviceGetCapsTest, givenRootDeviceWithSubDevicesWhenQueriedForCacheSizeThenValueIsMultiplied) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto singleRootDeviceCacheSize = deviceFactory.rootDevices[0]->deviceInfo.globalMemCacheSize;

    for (uint32_t subDevicesCount : {2, 3, 4}) {
        UltClDeviceFactory deviceFactory{1, subDevicesCount};
        auto rootDeviceCacheSize = deviceFactory.rootDevices[0]->deviceInfo.globalMemCacheSize;
        for (auto &subDevice : deviceFactory.rootDevices[0]->subDevices) {
            EXPECT_EQ(singleRootDeviceCacheSize, subDevice->getDeviceInfo().globalMemCacheSize);
            EXPECT_EQ(rootDeviceCacheSize, subDevice->getDeviceInfo().globalMemCacheSize * subDevicesCount);
        }
    }
}

TEST_F(DeviceGetCapsTest, givenClKhrExternalMemoryExtensionEnabledWhenCapsAreCreatedThenDeviceInfoReportsSupportOfExternalMemorySharing) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_TRUE(device->deviceInfo.externalMemorySharing);
}

TEST_F(DeviceGetCapsTest, whenInitializeCapsThenVmeIsNotSupported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    device->driverInfo.reset();
    device->name.clear();
    device->initializeCaps();
    cl_uint expectedVmeAvcVersion = CL_AVC_ME_VERSION_0_INTEL;
    cl_uint expectedVmeVersion = CL_ME_VERSION_LEGACY_INTEL;

    EXPECT_EQ(expectedVmeVersion, device->getDeviceInfo().vmeVersion);
    EXPECT_EQ(expectedVmeAvcVersion, device->getDeviceInfo().vmeAvcVersion);

    EXPECT_FALSE(device->getDeviceInfo().vmeAvcSupportsTextureSampler);
    EXPECT_FALSE(device->getDevice().getDeviceInfo().vmeAvcSupportsPreemption);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, DeviceGetCapsTest, givenCopyQueueWhenGettingQueueFamilyCapabilitiesThenDoNotReturnUnsupportedOperations) {
    const cl_command_queue_capabilities_intel capabilitiesNotSupportedOnBlitter = CL_QUEUE_CAPABILITY_KERNEL_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL |
                                                                                  CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL;
    const cl_command_queue_capabilities_intel expectedBlitterCapabilities = setBits(MockClDevice::getQueueFamilyCapabilitiesAll(), false, capabilitiesNotSupportedOnBlitter);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(expectedBlitterCapabilities, device->getQueueFamilyCapabilities(NEO::EngineGroupType::copy));
}

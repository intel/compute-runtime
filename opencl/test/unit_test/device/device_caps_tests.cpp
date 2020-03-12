/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_builtins.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"

#include "driver_version.h"
#include "gtest/gtest.h"

#include <memory>

namespace NEO {
extern const char *familyName[];
namespace MockSipData {
extern SipKernelType calledType;
extern bool called;
} // namespace MockSipData
} // namespace NEO

using namespace NEO;

struct DeviceGetCapsTest : public ::testing::Test {
    void SetUp() override {
        MockSipData::calledType = SipKernelType::COUNT;
        MockSipData::called = false;
    }
    void TearDown() override {
        MockSipData::calledType = SipKernelType::COUNT;
        MockSipData::called = false;
    }
};

TEST_F(DeviceGetCapsTest, WhenCreatingDeviceThenCapsArePopulatedCorrectly) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    const auto &sharedCaps = device->getSharedDeviceInfo();
    const auto &sysInfo = platformDevices[0]->gtSystemInfo;
    auto &hwHelper = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily);

    EXPECT_NE(nullptr, caps.builtInKernels);

    std::string strDriverName = caps.name;
    std::string strFamilyName = familyName[device->getRenderCoreFamily()];

    EXPECT_NE(std::string::npos, strDriverName.find(strFamilyName));

    EXPECT_NE(nullptr, caps.name);
    EXPECT_NE(nullptr, caps.vendor);
    EXPECT_NE(nullptr, caps.driverVersion);
    EXPECT_NE(nullptr, caps.profile);
    EXPECT_NE(nullptr, caps.clVersion);
    EXPECT_NE(nullptr, caps.clCVersion);

    EXPECT_NE(nullptr, caps.spirVersions);
    EXPECT_NE(nullptr, caps.deviceExtensions);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), caps.deviceAvailable);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), caps.compilerAvailable);
    EXPECT_EQ(16u, caps.preferredVectorWidthChar);
    EXPECT_EQ(8u, caps.preferredVectorWidthShort);
    EXPECT_EQ(4u, caps.preferredVectorWidthInt);
    EXPECT_EQ(1u, caps.preferredVectorWidthLong);
    EXPECT_EQ(1u, caps.preferredVectorWidthFloat);
    EXPECT_EQ(1u, caps.preferredVectorWidthDouble);
    EXPECT_EQ(8u, caps.preferredVectorWidthHalf);
    EXPECT_EQ(16u, caps.nativeVectorWidthChar);
    EXPECT_EQ(8u, caps.nativeVectorWidthShort);
    EXPECT_EQ(4u, caps.nativeVectorWidthInt);
    EXPECT_EQ(1u, caps.nativeVectorWidthLong);
    EXPECT_EQ(1u, caps.nativeVectorWidthFloat);
    EXPECT_EQ(1u, caps.nativeVectorWidthDouble);
    EXPECT_EQ(8u, caps.nativeVectorWidthHalf);
    EXPECT_EQ(1u, caps.linkerAvailable);
    EXPECT_NE(0u, sharedCaps.globalMemCachelineSize);
    EXPECT_NE(0u, caps.globalMemCacheSize);
    EXPECT_LT(0u, sharedCaps.globalMemSize);
    EXPECT_EQ(sharedCaps.maxMemAllocSize, caps.maxConstantBufferSize);
    EXPECT_NE(nullptr, sharedCaps.ilVersion);

    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), caps.deviceAvailable);
    EXPECT_EQ(static_cast<cl_device_mem_cache_type>(CL_READ_WRITE_CACHE), caps.globalMemCacheType);

    EXPECT_EQ(sysInfo.EUCount, caps.maxComputUnits);
    EXPECT_LT(0u, caps.maxConstantArgs);

    EXPECT_LE(128u, sharedCaps.maxReadImageArgs);
    EXPECT_LE(128u, sharedCaps.maxWriteImageArgs);
    EXPECT_EQ(128u, caps.maxReadWriteImageArgs);

    EXPECT_LE(sharedCaps.maxReadImageArgs * sizeof(cl_mem), sharedCaps.maxParameterSize);
    EXPECT_LE(sharedCaps.maxWriteImageArgs * sizeof(cl_mem), sharedCaps.maxParameterSize);
    EXPECT_LE(128u * MB, sharedCaps.maxMemAllocSize);
    EXPECT_GE((4 * GB) - (8 * KB), sharedCaps.maxMemAllocSize);
    EXPECT_LE(65536u, sharedCaps.imageMaxBufferSize);

    EXPECT_GT(sharedCaps.maxWorkGroupSize, 0u);
    EXPECT_EQ(sharedCaps.maxWorkItemSizes[0], sharedCaps.maxWorkGroupSize);
    EXPECT_EQ(sharedCaps.maxWorkItemSizes[1], sharedCaps.maxWorkGroupSize);
    EXPECT_EQ(sharedCaps.maxWorkItemSizes[2], sharedCaps.maxWorkGroupSize);
    EXPECT_LT(0u, sharedCaps.maxSamplers);

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

    auto expectedDeviceSubgroups = hwHelper.getDeviceSubGroupSizes();
    EXPECT_EQ(expectedDeviceSubgroups.size(), sharedCaps.maxSubGroups.size());

    for (uint32_t i = 0; i < expectedDeviceSubgroups.size(); i++) {
        EXPECT_EQ(expectedDeviceSubgroups[i], sharedCaps.maxSubGroups[i]);
    }

    if (device->getEnabledClVersion() >= 21) {
        EXPECT_TRUE(caps.independentForwardProgress != 0);
    } else {
        EXPECT_FALSE(caps.independentForwardProgress != 0);
    }

    EXPECT_EQ(sharedCaps.maxWorkGroupSize / hwHelper.getMinimalSIMDSize(), caps.maxNumOfSubGroups);

    EXPECT_EQ(1024u, caps.maxOnDeviceEvents);
    EXPECT_EQ(1u, sharedCaps.maxOnDeviceQueues);
    EXPECT_EQ(64u * MB, caps.queueOnDeviceMaxSize);
    EXPECT_EQ(128 * KB, caps.queueOnDevicePreferredSize);
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE),
              caps.queueOnDeviceProperties);

    EXPECT_EQ(64u, caps.preferredGlobalAtomicAlignment);
    EXPECT_EQ(64u, caps.preferredLocalAtomicAlignment);
    EXPECT_EQ(64u, caps.preferredPlatformAtomicAlignment);

    EXPECT_EQ(static_cast<cl_bool>(device->getHardwareInfo().capabilityTable.supportsImages), sharedCaps.imageSupport);
    EXPECT_EQ(16384u, sharedCaps.image2DMaxWidth);
    EXPECT_EQ(16384u, sharedCaps.image2DMaxHeight);
    EXPECT_EQ(2048u, sharedCaps.imageMaxArraySize);
    if (device->getHardwareInfo().capabilityTable.clVersionSupport == 12 && is64bit) {
        EXPECT_TRUE(sharedCaps.force32BitAddressess);
    }
}

HWTEST_F(DeviceGetCapsTest, givenDeviceWhenAskingForSubGroupSizesThenReturnCorrectValues) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    auto &hwHelper = HwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily);

    auto deviceSubgroups = hwHelper.getDeviceSubGroupSizes();

    EXPECT_EQ(3u, deviceSubgroups.size());
    EXPECT_EQ(8u, deviceSubgroups[0]);
    EXPECT_EQ(16u, deviceSubgroups[1]);
    EXPECT_EQ(32u, deviceSubgroups[2]);
}

TEST_F(DeviceGetCapsTest, GivenPlatformWhenGettingHwInfoThenImage3dDimensionsAreCorrect) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    const auto &sharedCaps = device->getSharedDeviceInfo();

    if (device->getHardwareInfo().platform.eRenderCoreFamily > IGFX_GEN8_CORE) {
        EXPECT_EQ(16384u, caps.image3DMaxWidth);
        EXPECT_EQ(16384u, caps.image3DMaxHeight);
    } else {
        EXPECT_EQ(2048u, caps.image3DMaxWidth);
        EXPECT_EQ(2048u, caps.image3DMaxHeight);
    }

    EXPECT_EQ(2048u, sharedCaps.image3DMaxDepth);
}

TEST_F(DeviceGetCapsTest, givenDontForcePreemptionModeDebugVariableWhenCreateDeviceThenSetDefaultHwPreemptionMode) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForcePreemptionMode.set(-1);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        EXPECT_TRUE(device->getHardwareInfo().capabilityTable.defaultPreemptionMode ==
                    device->getPreemptionMode());
    }
}

TEST_F(DeviceGetCapsTest, givenForcePreemptionModeDebugVariableWhenCreateDeviceThenSetForcedMode) {
    DebugManagerStateRestore dbgRestorer;
    {
        PreemptionMode forceMode = PreemptionMode::MidThread;
        if (platformDevices[0]->capabilityTable.defaultPreemptionMode == forceMode) {
            // force non-default mode
            forceMode = PreemptionMode::ThreadGroup;
        }
        DebugManager.flags.ForcePreemptionMode.set((int32_t)forceMode);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

        EXPECT_TRUE(forceMode == device->getPreemptionMode());
    }
}

TEST_F(DeviceGetCapsTest, givenDeviceWithMidThreadPreemptionWhenDeviceIsCreatedThenSipKernelIsNotCreated) {
    DebugManagerStateRestore dbgRestorer;
    {
        auto builtIns = new MockBuiltins();
        ASSERT_FALSE(MockSipData::called);

        DebugManager.flags.ForcePreemptionMode.set((int32_t)PreemptionMode::MidThread);

        auto executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0u]->builtins.reset(builtIns);
        auto device = std::unique_ptr<Device>(MockDevice::createWithExecutionEnvironment<MockDevice>(platformDevices[0], executionEnvironment, 0u));
        ASSERT_EQ(builtIns, device->getBuiltIns());
        EXPECT_FALSE(MockSipData::called);
    }
}

TEST_F(DeviceGetCapsTest, givenForceOclVersion21WhenCapsAreCreatedThenDeviceReportsOpenCL21) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(21);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
        EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST_F(DeviceGetCapsTest, givenForceOclVersion20WhenCapsAreCreatedThenDeviceReportsOpenCL20) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(20);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("OpenCL 2.0 NEO ", caps.clVersion);
        EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST_F(DeviceGetCapsTest, givenForceOclVersion12WhenCapsAreCreatedThenDeviceReportsOpenCL12) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(12);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
        EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST_F(DeviceGetCapsTest, givenForceInvalidOclVersionWhenCapsAreCreatedThenDeviceWillDefaultToOpenCL12) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(1);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
        EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST_F(DeviceGetCapsTest, givenForce32bitAddressingWhenCapsAreCreatedThenDeviceReports32bitAddressingOptimization) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.Force32bitAddressing.set(true);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        const auto &sharedCaps = device->getSharedDeviceInfo();
        if (is64bit) {
            EXPECT_TRUE(sharedCaps.force32BitAddressess);
        } else {
            EXPECT_FALSE(sharedCaps.force32BitAddressess);
        }
        auto expectedSize = (cl_ulong)(4 * 0.8 * GB);
        EXPECT_LE(sharedCaps.globalMemSize, expectedSize);
        EXPECT_LE(sharedCaps.maxMemAllocSize, expectedSize);
        EXPECT_LE(caps.maxConstantBufferSize, expectedSize);
        EXPECT_EQ(sharedCaps.addressBits, 32u);
    }
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenGlobalMemSizeIsAlignedDownToPageSize) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &sharedCaps = device->getSharedDeviceInfo();

    auto expectedSize = alignDown(sharedCaps.globalMemSize, MemoryConstants::pageSize);

    EXPECT_EQ(sharedCaps.globalMemSize, expectedSize);
}

TEST_F(DeviceGetCapsTest, Given32bitAddressingWhenDeviceIsCreatedThenGlobalMemSizeIsAlignedDownToPageSize) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &sharedCaps = device->getSharedDeviceInfo();
    auto pMemManager = device->getMemoryManager();
    unsigned int enabledCLVer = device->getEnabledClVersion();
    bool addressing32Bit = is32bit || (is64bit && (enabledCLVer < 20)) || DebugManager.flags.Force32bitAddressing.get();

    cl_ulong sharedMem = (cl_ulong)pMemManager->getSystemSharedMemory(0u);
    cl_ulong maxAppAddrSpace = (cl_ulong)pMemManager->getMaxApplicationAddress() + 1ULL;
    cl_ulong memSize = std::min(sharedMem, maxAppAddrSpace);
    memSize = (cl_ulong)((double)memSize * 0.8);
    if (addressing32Bit) {
        memSize = std::min(memSize, (uint64_t)(4 * GB * 0.8));
    }
    cl_ulong expectedSize = alignDown(memSize, MemoryConstants::pageSize);

    EXPECT_EQ(sharedCaps.globalMemSize, expectedSize);
}

TEST_F(DeviceGetCapsTest, givenDeviceCapsWhenLocalMemoryIsEnabledThenCalculateGlobalMemSizeBasedOnLocalMemory) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableLocalMemory.set(true);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &sharedCaps = device->getSharedDeviceInfo();
    auto pMemManager = device->getMemoryManager();
    auto enabledCLVer = device->getEnabledClVersion();
    bool addressing32Bit = is32bit || (is64bit && (enabledCLVer < 20)) || DebugManager.flags.Force32bitAddressing.get();

    auto localMem = pMemManager->getLocalMemorySize(0u);
    auto maxAppAddrSpace = pMemManager->getMaxApplicationAddress() + 1;
    auto memSize = std::min(localMem, maxAppAddrSpace);
    memSize = static_cast<cl_ulong>(memSize * 0.8);
    if (addressing32Bit) {
        memSize = std::min(memSize, static_cast<cl_ulong>(4 * GB * 0.8));
    }
    cl_ulong expectedSize = alignDown(memSize, MemoryConstants::pageSize);

    EXPECT_EQ(sharedCaps.globalMemSize, expectedSize);
}

TEST_F(DeviceGetCapsTest, givenGlobalMemSizeWhenCalculatingMaxAllocSizeThenAdjustToHWCap) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    HardwareCapabilities hwCaps = {0};
    auto &hwHelper = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily);
    hwHelper.setupHardwareCapabilities(&hwCaps, *platformDevices[0]);

    uint64_t expectedSize = std::max((caps.globalMemSize / 2), static_cast<uint64_t>(128ULL * MemoryConstants::megaByte));
    expectedSize = std::min(expectedSize, hwCaps.maxMemAllocSize);

    EXPECT_EQ(caps.maxMemAllocSize, expectedSize);
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenExtensionsStringEndsWithSpace) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    auto len = strlen(caps.deviceExtensions);
    ASSERT_LT(0U, len);
    EXPECT_EQ(' ', caps.deviceExtensions[len - 1]);
}

TEST_F(DeviceGetCapsTest, givenEnableSharingFormatQuerySetTrueAndDisabledMultipleSubDevicesWhenDeviceCapsAreCreatedThenSharingFormatQueryIsReported) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableFormatQuery.set(true);
    DebugManager.flags.CreateMultipleSubDevices.set(0);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_sharing_format_query ")));
}

TEST_F(DeviceGetCapsTest, givenEnableSharingFormatQuerySetTrueAndEnabledMultipleSubDevicesWhenDeviceCapsAreCreatedThenSharingFormatQueryIsNotReported) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableFormatQuery.set(true);
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, ::testing::Not(::testing::HasSubstr(std::string("cl_intel_sharing_format_query "))));
}

TEST_F(DeviceGetCapsTest, givenOpenCLVersion21WhenCapsAreCreatedThenDeviceReportsClKhrSubgroupsExtension) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(21);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_subgroups")));
}

TEST_F(DeviceGetCapsTest, givenOpenCLVersion20WhenCapsAreCreatedThenDeviceDoesntReportClKhrSubgroupsExtension) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(20);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_subgroups"))));
}

TEST_F(DeviceGetCapsTest, givenOpenCLVersion21WhenCapsAreCreatedThenDeviceReportsClKhrIlProgramExtension) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(21);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_il_program")));
}

TEST_F(DeviceGetCapsTest, givenOpenCLVersion20WhenCapsAreCreatedThenDeviceDoesntReportClKhrIlProgramExtension) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(20);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_il_program"))));
}

TEST_F(DeviceGetCapsTest, givenOpenCLVersion21WhenCapsAreCreatedThenDeviceReportsClIntelSpirvExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(21);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    const HardwareInfo *hwInfo = platformDevices[0];

    if (hwInfo->capabilityTable.supportsVme) {
        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_spirv_device_side_avc_motion_estimation")));
    } else {
        EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_spirv_device_side_avc_motion_estimation"))));
    }
    if (hwInfo->capabilityTable.supportsImages) {
        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_spirv_media_block_io")));
        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_3d_image_writes")));
    } else {
        EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_spirv_media_block_io"))));
        EXPECT_THAT(caps.deviceExtensions, testing::Not(std::string("cl_khr_3d_image_writes")));
    }
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_spirv_subgroups")));
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_spirv_no_integer_wrap_decoration")));
}

TEST_F(DeviceGetCapsTest, givenSupportImagesWhenCapsAreCreatedThenDeviceReportsClIntelSpirvMediaBlockIoExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(21);
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_spirv_media_block_io")));
}

TEST_F(DeviceGetCapsTest, givenNotSupportImagesWhenCapsAreCreatedThenDeviceNotReportsClIntelSpirvMediaBlockIoExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(21);
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_spirv_media_block_io"))));
}

TEST_F(DeviceGetCapsTest, givenSupportImagesWhenCapsAreCreatedThenDeviceReportsClKhr3dImageWritesExtensions) {
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_3d_image_writes")));
}

TEST_F(DeviceGetCapsTest, givenNotSupportImagesWhenCapsAreCreatedThenDeviceNotReportsClKhr3dImageWritesExtensions) {
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_3d_image_writes"))));
}

TEST_F(DeviceGetCapsTest, givenOpenCLVersion12WhenCapsAreCreatedThenDeviceDoesntReportClIntelSpirvExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(12);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_spirv_device_side_avc_motion_estimation"))));
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_spirv_subgroups"))));
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_spirv_no_integer_wrap_decoration"))));
}

TEST_F(DeviceGetCapsTest, givenEnableNV12setToTrueAndSupportImagesWhenCapsAreCreatedThenDeviceReportsNV12Extension) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableNV12.set(true);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    if (device->getHardwareInfo().capabilityTable.supportsImages) {
        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_planar_yuv")));
        EXPECT_TRUE(caps.nv12Extension);
    } else {
        EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_planar_yuv"))));
    }
}

TEST_F(DeviceGetCapsTest, givenEnablePackedYuvsetToTrueAndSupportImagesWhenCapsAreCreatedThenDeviceReportsPackedYuvExtension) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnablePackedYuv.set(true);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    if (device->getHardwareInfo().capabilityTable.supportsImages) {
        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_packed_yuv")));
        EXPECT_TRUE(caps.packedYuvExtension);
    } else {
        EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_packed_yuv"))));
    }
}

TEST_F(DeviceGetCapsTest, givenSupportImagesWhenCapsAreCreatedThenDeviceReportsPackedYuvAndNV12Extensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnablePackedYuv.set(true);
    DebugManager.flags.EnableNV12.set(true);
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_packed_yuv")));
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_planar_yuv")));
}

TEST_F(DeviceGetCapsTest, givenNotSupportImagesWhenCapsAreCreatedThenDeviceNotReportsPackedYuvAndNV12Extensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnablePackedYuv.set(true);
    DebugManager.flags.EnableNV12.set(true);
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_packed_yuv"))));
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_planar_yuv"))));
}

TEST_F(DeviceGetCapsTest, givenEnableNV12setToFalseWhenCapsAreCreatedThenDeviceDoesNotReportNV12Extension) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableNV12.set(false);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_planar_yuv"))));
    EXPECT_FALSE(caps.nv12Extension);
}

TEST_F(DeviceGetCapsTest, givenEnablePackedYuvsetToFalseWhenCapsAreCreatedThenDeviceDoesNotReportPackedYuvExtension) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnablePackedYuv.set(false);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_packed_yuv"))));
    EXPECT_FALSE(caps.packedYuvExtension);
}

TEST_F(DeviceGetCapsTest, givenEnableVmeSetToTrueAndDeviceSupportsVmeWhenCapsAreCreatedThenDeviceReportsVmeExtensionAndBuiltins) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableIntelVme.set(1);
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsVme = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_motion_estimation")));
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_device_side_avc_motion_estimation")));
    EXPECT_TRUE(caps.vmeExtension);

    EXPECT_THAT(caps.builtInKernels, testing::HasSubstr("block_motion_estimate_intel"));
}

TEST_F(DeviceGetCapsTest, givenEnableVmeSetToTrueAndDeviceDoesNotSupportVmeWhenCapsAreCreatedThenDeviceReportsVmeExtensionAndBuiltins) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableIntelVme.set(1);
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsVme = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_motion_estimation")));
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_device_side_avc_motion_estimation")));
    EXPECT_TRUE(caps.vmeExtension);

    EXPECT_THAT(caps.builtInKernels, testing::HasSubstr("block_motion_estimate_intel"));
}

TEST_F(DeviceGetCapsTest, givenEnableVmeSetToFalseAndDeviceDoesNotSupportVmeWhenCapsAreCreatedThenDeviceDoesNotReportVmeExtensionAndBuiltins) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableIntelVme.set(0);
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsVme = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_motion_estimation"))));
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_device_side_avc_motion_estimation"))));
    EXPECT_FALSE(caps.vmeExtension);

    EXPECT_THAT(caps.builtInKernels, testing::Not(testing::HasSubstr("block_motion_estimate_intel")));
}

TEST_F(DeviceGetCapsTest, givenEnableVmeSetToFalseAndDeviceSupportsVmeWhenCapsAreCreatedThenDeviceDoesNotReportVmeExtensionAndBuiltins) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableIntelVme.set(0);
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsVme = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_motion_estimation"))));
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_device_side_avc_motion_estimation"))));
    EXPECT_FALSE(caps.vmeExtension);

    EXPECT_THAT(caps.builtInKernels, testing::Not(testing::HasSubstr("block_motion_estimate_intel")));
}

TEST_F(DeviceGetCapsTest, givenEnableAdvancedVmeSetToTrueAndDeviceSupportsVmeWhenCapsAreCreatedThenDeviceReportsAdvancedVmeExtensionAndBuiltins) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableIntelAdvancedVme.set(1);
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsVme = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_advanced_motion_estimation")));

    EXPECT_THAT(caps.builtInKernels, testing::HasSubstr("block_advanced_motion_estimate_check_intel"));
    EXPECT_THAT(caps.builtInKernels, testing::HasSubstr("block_advanced_motion_estimate_bidirectional_check_intel"));
}

TEST_F(DeviceGetCapsTest, givenDeviceCapsSupportFor64BitAtomicsFollowsHardwareCapabilities) {
    auto hwInfo = *platformDevices[0];
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    if (hwInfo.capabilityTable.ftrSupportsInteger64BitAtomics) {
        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr("cl_khr_int64_base_atomics "));
        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr("cl_khr_int64_extended_atomics "));
    } else {
        EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr("cl_khr_int64_base_atomics ")));
        EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr("cl_khr_int64_extended_atomics ")));
    }
}

TEST_F(DeviceGetCapsTest, givenEnableAdvancedVmeSetToTrueAndDeviceDoesNotSupportVmeWhenCapsAreCreatedThenDeviceReportAdvancedVmeExtensionAndBuiltins) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableIntelAdvancedVme.set(1);
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsVme = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_advanced_motion_estimation")));

    EXPECT_THAT(caps.builtInKernels, testing::HasSubstr("block_advanced_motion_estimate_check_intel"));
    EXPECT_THAT(caps.builtInKernels, testing::HasSubstr("block_advanced_motion_estimate_bidirectional_check_intel"));
}

TEST_F(DeviceGetCapsTest, givenEnableAdvancedVmeSetToFalseAndDeviceDoesNotSupportVmeWhenCapsAreCreatedThenDeviceDoesNotReportAdvancedVmeExtensionAndBuiltins) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableIntelAdvancedVme.set(0);
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsVme = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_advanced_motion_estimation"))));

    EXPECT_THAT(caps.builtInKernels, testing::Not(testing::HasSubstr("block_advanced_motion_estimate_check_intel")));
    EXPECT_THAT(caps.builtInKernels, testing::Not(testing::HasSubstr("block_advanced_motion_estimate_bidirectional_check_intel")));
}

TEST_F(DeviceGetCapsTest, givenEnableAdvancedVmeSetToFalseAndDeviceSupportsVmeWhenCapsAreCreatedThenDeviceDoesNotReportAdvancedVmeExtensionAndBuiltins) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableIntelAdvancedVme.set(0);
    auto hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsVme = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_advanced_motion_estimation"))));

    EXPECT_THAT(caps.builtInKernels, testing::Not(testing::HasSubstr("block_advanced_motion_estimate_check_intel")));
    EXPECT_THAT(caps.builtInKernels, testing::Not(testing::HasSubstr("block_advanced_motion_estimate_bidirectional_check_intel")));
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenVmeIsEnabled) {
    DebugSettingsManager<DebugFunctionalityLevel::RegKeys> freshDebugSettingsManager("");
    EXPECT_TRUE(freshDebugSettingsManager.flags.EnableIntelVme.get());
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenPriorityHintsExtensionIsReported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_priority_hints")));
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenCreateCommandQueueExtensionIsReported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_create_command_queue")));
}

TEST_F(DeviceGetCapsTest, WhenDeviceIsCreatedThenThrottleHintsExtensionIsReported) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_throttle_hints")));
}

TEST_F(DeviceGetCapsTest, GivenAnyDeviceWhenCheckingExtensionsThenSupportSubgroupsChar) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_subgroups_char")));
}

TEST_F(DeviceGetCapsTest, GivenAnyDeviceWhenCheckingExtensionsThenSupportSubgroupsLong) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_subgroups_long")));
}

TEST_F(DeviceGetCapsTest, givenAtleastOCL2DeviceThenExposesMipMapAndUnifiedMemoryExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(20);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;
    if (device->getHardwareInfo().capabilityTable.supportsImages) {
        EXPECT_THAT(extensionString, testing::HasSubstr(std::string("cl_khr_mipmap_image")));
        EXPECT_THAT(extensionString, testing::HasSubstr(std::string("cl_khr_mipmap_image_writes")));
    } else {
        EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_mipmap_image")));
        EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_mipmap_image_writes")));
    }
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_unified_shared_memory_preview")));
}

TEST_F(DeviceGetCapsTest, givenSupportImagesWhenCapsAreCreatedThenDeviceReportsMinMapExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(20);
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_mipmap_image")));
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_mipmap_image_writes")));
}

TEST_F(DeviceGetCapsTest, givenNotSupportImagesWhenCapsAreCreatedThenDeviceNotReportsMinMapExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(20);
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto &caps = device->getDeviceInfo();
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_mipmap_image"))));
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_mipmap_image_writes"))));
}

TEST_F(DeviceGetCapsTest, givenOCL12DeviceThenDoesNotExposesMipMapAndUnifiedMemoryExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(12);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_mipmap_image"))));
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_khr_mipmap_image_writes"))));
    EXPECT_THAT(caps.deviceExtensions, testing::Not(testing::HasSubstr(std::string("cl_intel_unified_shared_memory_preview"))));
}

TEST_F(DeviceGetCapsTest, givenSupporteImagesWhenCreateExtentionsListThenDeviceReportsImagesExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(20);
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto extensions = device->getDeviceInfo().deviceExtensions;

    EXPECT_THAT(extensions, testing::HasSubstr(std::string("cl_khr_image2d_from_buffer")));
    EXPECT_THAT(extensions, testing::HasSubstr(std::string("cl_khr_depth_images")));
    EXPECT_THAT(extensions, testing::HasSubstr(std::string("cl_intel_media_block_io")));
}

TEST_F(DeviceGetCapsTest, givenNotSupporteImagesWhenCreateExtentionsListThenDeviceNotReportsImagesExtensions) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceOCLVersion.set(20);
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.capabilityTable.supportsImages = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    const auto extensions = device->getDeviceInfo().deviceExtensions;

    EXPECT_THAT(extensions, testing::Not(testing::HasSubstr(std::string("cl_khr_image2d_from_buffer"))));
    EXPECT_THAT(extensions, testing::Not(testing::HasSubstr(std::string("cl_khr_depth_images"))));
    EXPECT_THAT(extensions, testing::Not(testing::HasSubstr(std::string("cl_intel_media_block_io"))));
}

TEST_F(DeviceGetCapsTest, givenDeviceThatDoesntHaveFp64ThenExtensionIsNotReported) {
    HardwareInfo nonFp64Device = *platformDevices[0];
    nonFp64Device.capabilityTable.ftrSupportsFP64 = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&nonFp64Device));

    const auto &caps = device->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;
    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

TEST_F(DeviceGetCapsTest, givenDeviceWhenGettingHostUnifiedMemoryCapThenItDependsOnLocalMemory) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    const auto &caps = device->getDeviceInfo();

    auto &hwHelper = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily);
    auto localMemoryEnabled = hwHelper.isLocalMemoryEnabled(*platformDevices[0]);

    EXPECT_EQ((localMemoryEnabled == false), caps.hostUnifiedMemory);
}

TEST(DeviceGetCaps, givenDeviceThatDoesntHaveFp64WhenDbgFlagEnablesFp64ThenReportFp64Flags) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.OverrideDefaultFP64Settings.set(1);
    HardwareInfo nonFp64Device = *platformDevices[0];
    nonFp64Device.capabilityTable.ftrSupportsFP64 = false;
    nonFp64Device.capabilityTable.ftrSupports64BitMath = false;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&nonFp64Device));

    const auto &caps = device->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;
    EXPECT_NE(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_NE(0u, caps.doubleFpConfig);
    cl_device_fp_config actualSingleFp = caps.singleFpConfig & static_cast<cl_device_fp_config>(CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
    cl_device_fp_config expectedSingleFp = static_cast<cl_device_fp_config>(CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
    EXPECT_EQ(expectedSingleFp, actualSingleFp);
}

TEST(DeviceGetCaps, givenDeviceThatDoesHaveFp64WhenDbgFlagDisablesFp64ThenDontReportFp64Flags) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.OverrideDefaultFP64Settings.set(0);
    HardwareInfo fp64Device = *platformDevices[0];
    fp64Device.capabilityTable.ftrSupportsFP64 = true;
    fp64Device.capabilityTable.ftrSupports64BitMath = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&fp64Device));

    const auto &caps = device->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;
    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
    cl_device_fp_config actualSingleFp = caps.singleFpConfig & static_cast<cl_device_fp_config>(CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
    cl_device_fp_config notExpectedSingleFp = static_cast<cl_device_fp_config>(CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
    EXPECT_NE(notExpectedSingleFp, actualSingleFp);
}

TEST(DeviceGetCaps, givenOclVersionLessThan21WhenCapsAreCreatedThenDeviceReportsNoSupportedIlVersions) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(12);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("", caps.ilVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST(DeviceGetCaps, givenOclVersion21WhenCapsAreCreatedThenDeviceReportsSpirvAsSupportedIl) {
    DebugManagerStateRestore dbgRestorer;
    {
        DebugManager.flags.ForceOCLVersion.set(21);
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        const auto &caps = device->getDeviceInfo();
        EXPECT_STREQ("SPIR-V_1.2 ", caps.ilVersion);
        DebugManager.flags.ForceOCLVersion.set(0);
    }
}

TEST(DeviceGetCaps, givenDisabledFtrPooledEuWhenCalculatingMaxEuPerSSThenIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *platformDevices[0];
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.ftrPooledEuEnabled = 0;

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    auto expectedMaxWGS = (mySysInfo.EUCount / mySysInfo.SubSliceCount) *
                          (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 1024u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

HWTEST_F(DeviceGetCapsTest, givenEnabledFtrPooledEuWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *platformDevices[0];
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.ftrPooledEuEnabled = 1;

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    auto expectedMaxWGS = mySysInfo.EuCountPerPoolMin * (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 1024u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

TEST(DeviceGetCaps, givenDebugFlagToUseMaxSimdSizeForWkgCalculationWhenDeviceCapsAreCreatedThen1024WorkgroupSizeIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.set(true);

    HardwareInfo myHwInfo = *platformDevices[0];
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;

    mySysInfo.EUCount = 24;
    mySysInfo.SubSliceCount = 3;
    mySysInfo.ThreadCount = 24 * 7;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(1024u, device->getSharedDeviceInfo().maxWorkGroupSize);
    EXPECT_EQ(device->getSharedDeviceInfo().maxWorkGroupSize / 32, device->getDeviceInfo().maxNumOfSubGroups);
}

TEST(DeviceGetCaps, givenDebugFlagToUseCertainWorkgroupSizeWhenDeviceIsCreatedItHasCeratinWorkgroupSize) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.OverrideMaxWorkgroupSize.set(16u);

    HardwareInfo myHwInfo = *platformDevices[0];
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;

    mySysInfo.EUCount = 24;
    mySysInfo.SubSliceCount = 3;
    mySysInfo.ThreadCount = 24 * 7;
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(16u, device->getDeviceInfo().maxWorkGroupSize);
}

HWTEST_F(DeviceGetCapsTest, givenDeviceThatHasHighNumberOfExecutionUnitsWhenMaxWorkgroupSizeIsComputedItIsLimitedTo1024) {
    HardwareInfo myHwInfo = *platformDevices[0];
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    auto &hwHelper = HwHelper::get(myHwInfo.platform.eRenderCoreFamily);

    mySysInfo.EUCount = 32;
    mySysInfo.SubSliceCount = 2;
    mySysInfo.ThreadCount = 32 * hwHelper.getMinimalSIMDSize(); // 128 threads per subslice, in simd 8 gives 1024
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(1024u, device->getSharedDeviceInfo().maxWorkGroupSize);
    EXPECT_EQ(device->getSharedDeviceInfo().maxWorkGroupSize / hwHelper.getMinimalSIMDSize(), device->getDeviceInfo().maxNumOfSubGroups);
}

class DriverInfoMock : public DriverInfo {
  public:
    DriverInfoMock(){};

    const static std::string testDeviceName;
    const static std::string testVersion;

    std::string getDeviceName(std::string defaultName) override { return testDeviceName; };
    std::string getVersion(std::string defaultVersion) override { return testVersion; };
};

const std::string DriverInfoMock::testDeviceName = "testDeviceName";
const std::string DriverInfoMock::testVersion = "testVersion";

TEST_F(DeviceGetCapsTest, givenSystemWithDriverInfoWhenGettingNameAndVersionThenReturnValuesFromDriverInfo) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    DriverInfoMock *driverInfoMock = new DriverInfoMock();
    device->driverInfo.reset(driverInfoMock);
    device->initializeCaps();

    const auto &caps = device->getDeviceInfo();

    EXPECT_STREQ(DriverInfoMock::testDeviceName.c_str(), caps.name);
    EXPECT_STREQ(DriverInfoMock::testVersion.c_str(), caps.driverVersion);
}

TEST_F(DeviceGetCapsTest, givenSystemWithNoDriverInfoWhenGettingNameAndVersionThenReturnDefaultValues) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    device->driverInfo.reset();
    device->name.clear();
    device->initializeCaps();

    const auto &caps = device->getDeviceInfo();

    std::string tempName = "Intel(R) ";
    tempName += familyName[platformDevices[0]->platform.eRenderCoreFamily];
    tempName += " HD Graphics NEO";

#define QTR(a) #a
#define TOSTR(b) QTR(b)
    const std::string expectedVersion = TOSTR(NEO_DRIVER_VERSION);
#undef QTR
#undef TOSTR

    EXPECT_STREQ(tempName.c_str(), caps.name);
    EXPECT_STREQ(expectedVersion.c_str(), caps.driverVersion);
}

TEST_F(DeviceGetCapsTest, GivenFlagEnabled64kbPagesWhenSetThenReturnCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    VariableBackup<bool> OsEnabled64kbPagesBackup(&OSInterface::osEnabled64kbPages);

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto &capabilityTable = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable;
    std::unique_ptr<MemoryManager> memoryManager;

    DebugManager.flags.Enable64kbpages.set(-1);

    capabilityTable.ftr64KBpages = false;
    OSInterface::osEnabled64kbPages = false;
    memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    capabilityTable.ftr64KBpages = false;
    OSInterface::osEnabled64kbPages = true;
    memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    capabilityTable.ftr64KBpages = true;
    OSInterface::osEnabled64kbPages = false;
    memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    capabilityTable.ftr64KBpages = true;
    OSInterface::osEnabled64kbPages = true;
    memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_TRUE(memoryManager->peek64kbPagesEnabled(0u));

    DebugManager.flags.Enable64kbpages.set(0); // force false
    memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_FALSE(memoryManager->peek64kbPagesEnabled(0u));

    DebugManager.flags.Enable64kbpages.set(1); // force true
    memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    EXPECT_TRUE(memoryManager->peek64kbPagesEnabled(0u));
}

TEST_F(DeviceGetCapsTest, givenUnifiedMemoryShardeSystemFlagWhenDeviceIsCreatedItContainsProperSystemMemorySetting) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableSharedSystemUsmSupport.set(0u);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    EXPECT_EQ(0u, device->getDeviceInfo().sharedSystemMemCapabilities);
    EXPECT_FALSE(device->areSharedSystemAllocationsAllowed());

    DebugManager.flags.EnableSharedSystemUsmSupport.set(1u);
    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0])});
    cl_unified_shared_memory_capabilities_intel expectedProperties = CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL;
    EXPECT_EQ(expectedProperties, device->getDeviceInfo().sharedSystemMemCapabilities);
    EXPECT_TRUE(device->areSharedSystemAllocationsAllowed());
}

TEST_F(DeviceGetCapsTest, givenDeviceWithNullSourceLevelDebuggerWhenCapsAreInitializedThenSourceLevelDebuggerActiveIsSetToFalse) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    const auto &caps = device->getDeviceInfo();
    EXPECT_EQ(nullptr, device->getDebugger());
    EXPECT_FALSE(caps.debuggerActive);
}

TEST(Device_UseCaps, givenCapabilityTableWhenDeviceInitializeCapsThenVmeVersionsAreSetProperly) {
    HardwareInfo hwInfo = *platformDevices[0];

    cl_uint expectedVmeVersion = CL_ME_VERSION_ADVANCED_VER_2_INTEL;
    cl_uint expectedVmeAvcVersion = CL_AVC_ME_VERSION_1_INTEL;

    hwInfo.capabilityTable.supportsVme = 0;
    hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler = 0;
    hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption = 0;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    {
        auto &caps = device->getDeviceInfo();
        auto &sharedCaps = device->getSharedDeviceInfo();
        EXPECT_EQ(0u, caps.vmeVersion);
        EXPECT_EQ(0u, caps.vmeAvcVersion);
        EXPECT_EQ(hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption, sharedCaps.vmeAvcSupportsPreemption);
        EXPECT_EQ(hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler, caps.vmeAvcSupportsTextureSampler);
    }

    hwInfo.capabilityTable.supportsVme = 1;
    hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler = 1;
    hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption = 1;

    device.reset(new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo)});

    {
        auto &caps = device->getDeviceInfo();
        auto &sharedCaps = device->getSharedDeviceInfo();
        EXPECT_EQ(expectedVmeVersion, caps.vmeVersion);
        EXPECT_EQ(expectedVmeAvcVersion, caps.vmeAvcVersion);
        EXPECT_EQ(hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption, sharedCaps.vmeAvcSupportsPreemption);
        EXPECT_EQ(hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler, caps.vmeAvcSupportsTextureSampler);
    }
}

typedef HwHelperTest DeviceCapsWithModifiedHwInfoTest;

TEST_F(DeviceCapsWithModifiedHwInfoTest, givenPlatformWithSourceLevelDebuggerNotSupportedWhenDeviceIsCreatedThenSourceLevelDebuggerActiveIsSetToFalse) {

    hardwareInfo.capabilityTable.debuggerSupported = false;

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));

    const auto &caps = device->getDeviceInfo();
    EXPECT_EQ(nullptr, device->getDebugger());
    EXPECT_FALSE(caps.debuggerActive);
}

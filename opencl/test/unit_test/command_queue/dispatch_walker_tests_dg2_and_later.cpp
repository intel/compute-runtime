/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/linear_stream_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/test/unit_test/command_queue/hardware_interface_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
using namespace NEO;

struct Dg2AndLaterDispatchWalkerBasicFixture : public LinearStreamFixture {
    void setUp() {
        LinearStreamFixture::setUp();
        memset(globalOffsets, 0, sizeof(globalOffsets));
        memset(startWorkGroups, 0, sizeof(startWorkGroups));
        memset(&threadPayload, 0, sizeof(threadPayload));

        localWorkSizesIn[0] = 16;
        localWorkSizesIn[1] = localWorkSizesIn[2] = 1;
        numWorkGroups[0] = numWorkGroups[1] = numWorkGroups[2] = 1;
        simd = 16;

        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), rootDeviceIndex));
        context = std::make_unique<MockContext>(device.get());
        kernel = std::make_unique<MockKernelWithInternals>(*device, context.get());
        sizeGrf = device->getHardwareInfo().capabilityTable.grfSize;
        sizeGrfDwords = sizeGrf / sizeof(uint32_t);

        for (uint32_t i = 0; i < sizeGrfDwords; i++) {
            crossThreadDataGrf[i] = i;
            crossThreadDataTwoGrf[i] = i + 2;
        }
        for (uint32_t i = sizeGrfDwords; i < sizeGrfDwords * 2; i++) {
            crossThreadDataTwoGrf[i] = i + 2;
        }
    }

    size_t globalOffsets[3];
    size_t startWorkGroups[3];
    size_t numWorkGroups[3];
    size_t localWorkSizesIn[3];
    uint32_t simd;
    uint32_t sizeGrf;
    uint32_t sizeInlineData;
    uint32_t sizeGrfDwords;
    uint32_t crossThreadDataGrf[16];
    uint32_t crossThreadDataTwoGrf[32];
    iOpenCL::SPatchThreadPayload threadPayload;

    const uint32_t rootDeviceIndex = 1u;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockKernelWithInternals> kernel;
};

using WalkerDispatchTestDg2AndLater = ::testing::Test;
using Dg2AndLaterDispatchWalkerBasicTest = Test<Dg2AndLaterDispatchWalkerBasicFixture>;
using matcherDG2AndLater = IsAtLeastXeHpgCore;

HWTEST2_F(WalkerDispatchTestDg2AndLater, whenProgramComputeWalkerThenApplyL3WAForSpecificPlatformAndRevision, matcherDG2AndLater) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    auto hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EncodeWalkerArgs walkerArgs{KernelExecutionType::Default, true};
    {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, walkerArgs);

        EXPECT_FALSE(walkerCmd.getL3PrefetchDisable());
    }

    {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, walkerArgs);

        if (hwInfo.platform.eProductFamily == IGFX_DG2) {
            EXPECT_TRUE(walkerCmd.getL3PrefetchDisable());
        } else {
            EXPECT_FALSE(walkerCmd.getL3PrefetchDisable());
        }
    }
}

HWTEST2_F(WalkerDispatchTestDg2AndLater, givenDebugVariableSetWhenProgramComputeWalkerThenApplyL3PrefetchAppropriately, matcherDG2AndLater) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    DebugManagerStateRestore restore;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    auto hwInfo = *defaultHwInfo;

    EncodeWalkerArgs walkerArgs{KernelExecutionType::Default, true};
    for (auto forceL3PrefetchForComputeWalker : {false, true}) {
        DebugManager.flags.ForceL3PrefetchForComputeWalker.set(forceL3PrefetchForComputeWalker);
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(hwInfo, walkerCmd, walkerArgs);
        EXPECT_EQ(!forceL3PrefetchForComputeWalker, walkerCmd.getL3PrefetchDisable());
    }
}

HWTEST2_F(Dg2AndLaterDispatchWalkerBasicTest, givenTimestampPacketWhenDispatchingThenProgramPostSyncData, matcherDG2AndLater) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    MockKernelWithInternals kernel1(*device);
    MockKernelWithInternals kernel2(*device);

    device->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    TimestampPacketContainer timestampPacketContainer;
    timestampPacketContainer.add(device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    timestampPacketContainer.add(device->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());

    MockMultiDispatchInfo multiDispatchInfo(device.get(), std::vector<Kernel *>({kernel1.mockKernel, kernel2.mockKernel}));

    MockCommandQueue cmdQ(context.get(), device.get(), nullptr, false);
    auto &cmdStream = cmdQ.getCS(0);

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);
    walkerArgs.currentTimestampPacketNodes = &timestampPacketContainer;
    HardwareInterface<FamilyType>::dispatchWalker(
        cmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        walkerArgs);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto gmmHelper = device->getGmmHelper();

    auto expectedMocs = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo) ? gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) : gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);

    auto walker = genCmdCast<COMPUTE_WALKER *>(*hwParser.itorWalker);
    EXPECT_EQ(FamilyType::POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, walker->getPostSync().getOperation());
    EXPECT_TRUE(walker->getPostSync().getDataportPipelineFlush());
    EXPECT_TRUE(walker->getPostSync().getDataportSubsliceCacheFlush());
    EXPECT_EQ(expectedMocs, walker->getPostSync().getMocs());
    auto contextStartAddress = TimestampPacketHelper::getContextStartGpuAddress(*timestampPacketContainer.peekNodes()[0]);
    EXPECT_EQ(contextStartAddress, walker->getPostSync().getDestinationAddress());

    auto secondWalkerItor = find<COMPUTE_WALKER *>(++hwParser.itorWalker, hwParser.cmdList.end());
    auto secondWalker = genCmdCast<COMPUTE_WALKER *>(*secondWalkerItor);

    EXPECT_EQ(FamilyType::POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP, secondWalker->getPostSync().getOperation());
    EXPECT_TRUE(secondWalker->getPostSync().getDataportPipelineFlush());
    EXPECT_TRUE(secondWalker->getPostSync().getDataportSubsliceCacheFlush());
    EXPECT_EQ(expectedMocs, walker->getPostSync().getMocs());
    contextStartAddress = TimestampPacketHelper::getContextStartGpuAddress(*timestampPacketContainer.peekNodes()[1]);
    EXPECT_EQ(contextStartAddress, secondWalker->getPostSync().getDestinationAddress());
}

HWTEST2_F(Dg2AndLaterDispatchWalkerBasicTest, givenDebugVariableEnabledWhenEnqueueingThenWriteWalkerStamp, matcherDG2AndLater) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableTimestampPacket.set(true);

    auto testDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext testContext(testDevice.get());
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(&testContext, testDevice.get(), nullptr);
    MockKernelWithInternals testKernel(*testDevice, &testContext);

    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(testKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_NE(nullptr, cmdQ->timestampPacketContainer.get());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto walker = genCmdCast<COMPUTE_WALKER *>(*hwParser.itorWalker);

    auto gmmHelper = device->getGmmHelper();
    auto expectedMocs = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo) ? gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) : gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);

    auto &postSyncData = walker->getPostSync();
    EXPECT_EQ(FamilyType::POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP,
              postSyncData.getOperation());
    EXPECT_TRUE(postSyncData.getDataportPipelineFlush());
    EXPECT_TRUE(postSyncData.getDataportSubsliceCacheFlush());
    EXPECT_EQ(expectedMocs, postSyncData.getMocs());
}

/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

using CommandQueuePvcAndLaterTests = ::testing::Test;

HWTEST2_F(CommandQueuePvcAndLaterTests, givenMultipleBcsEnginesWhenGetBcsCommandStreamReceiverIsCalledThenReturnProperCsrs, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    MockContext context{&clDevice};

    MockCommandQueue queue{context};
    queue.clearBcsEngines();
    ASSERT_EQ(0u, queue.countBcsEngines());
    queue.insertBcsEngine(aub_stream::EngineType::ENGINE_BCS);
    queue.insertBcsEngine(aub_stream::EngineType::ENGINE_BCS3);
    queue.insertBcsEngine(aub_stream::EngineType::ENGINE_BCS7);
    ASSERT_EQ(3u, queue.countBcsEngines());

    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->getOsContext().getEngineType());
    EXPECT_EQ(nullptr, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS1));
    EXPECT_EQ(nullptr, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS2));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS3, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS3)->getOsContext().getEngineType());
    EXPECT_EQ(nullptr, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS4));
    EXPECT_EQ(nullptr, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS5));
    EXPECT_EQ(nullptr, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS6));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS7, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS7)->getOsContext().getEngineType());
    EXPECT_EQ(nullptr, queue.getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS8));
}

HWTEST2_F(CommandQueuePvcAndLaterTests, givenAdditionalBcsWhenCreatingCommandQueueThenUseCorrectEngine, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    MockContext context{&clDevice};

    const auto familyIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::LinkedCopy);
    cl_command_queue_properties queueProperties[5] = {
        CL_QUEUE_FAMILY_INTEL,
        familyIndex,
        CL_QUEUE_INDEX_INTEL,
        0,
        0,
    };

    queueProperties[3] = 0;
    auto queue = std::make_unique<MockCommandQueue>(&context, context.getDevice(0), queueProperties, false);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, queue->bcsEngines[EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS1)]->getEngineType());
    EXPECT_EQ(1u, queue->countBcsEngines());

    queueProperties[3] = 4;
    queue = std::make_unique<MockCommandQueue>(&context, context.getDevice(0), queueProperties, false);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS5, queue->bcsEngines[EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS5)]->getEngineType());
    EXPECT_EQ(1u, queue->countBcsEngines());

    queueProperties[3] = 7;
    queue = std::make_unique<MockCommandQueue>(&context, context.getDevice(0), queueProperties, false);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS8, queue->bcsEngines[EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS8)]->getEngineType());
    EXPECT_EQ(1u, queue->countBcsEngines());
}

HWTEST2_F(CommandQueuePvcAndLaterTests, givenDeferCmdQBcsInitializationEnabledWhenCreateCommandQueueThenBcsCountIsZero, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    DebugManager.flags.DeferCmdQBcsInitialization.set(1u);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    cl_device_id clDeviceId = static_cast<cl_device_id>(&clDevice);
    ClDeviceVector clDevices{&clDeviceId, 1u};
    cl_int retVal{};
    auto context = std::unique_ptr<Context>{Context::create<Context>(nullptr, clDevices, nullptr, nullptr, retVal)};
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto queue = std::make_unique<MockCommandQueue>(*context);

    EXPECT_EQ(0u, queue->countBcsEngines());
}

HWTEST2_F(CommandQueuePvcAndLaterTests, whenConstructBcsEnginesForSplitThenContainsMultipleBcsEngines, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    DebugManager.flags.DeferCmdQBcsInitialization.set(1u);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    cl_device_id clDeviceId = static_cast<cl_device_id>(&clDevice);
    ClDeviceVector clDevices{&clDeviceId, 1u};
    cl_int retVal{};
    auto context = std::unique_ptr<Context>{Context::create<Context>(nullptr, clDevices, nullptr, nullptr, retVal)};
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto queue = std::make_unique<MockCommandQueue>(*context);
    EXPECT_EQ(0u, queue->countBcsEngines());

    queue->constructBcsEnginesForSplit();

    EXPECT_EQ(4u, queue->countBcsEngines());

    queue->constructBcsEnginesForSplit();

    EXPECT_EQ(4u, queue->countBcsEngines());
}

HWTEST2_F(CommandQueuePvcAndLaterTests, givenSplitBcsMaskWhenConstructBcsEnginesForSplitThenContainsGivenBcsEngines, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    std::bitset<bcsInfoMaskSize> bcsMask = 0b100110101;
    DebugManager.flags.DeferCmdQBcsInitialization.set(1u);
    DebugManager.flags.SplitBcsMask.set(static_cast<int>(bcsMask.to_ulong()));
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    cl_device_id clDeviceId = static_cast<cl_device_id>(&clDevice);
    ClDeviceVector clDevices{&clDeviceId, 1u};
    cl_int retVal{};
    auto context = std::unique_ptr<Context>{Context::create<Context>(nullptr, clDevices, nullptr, nullptr, retVal)};
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto queue = std::make_unique<MockCommandQueue>(*context);
    EXPECT_EQ(0u, queue->countBcsEngines());

    queue->constructBcsEnginesForSplit();

    EXPECT_EQ(5u, queue->countBcsEngines());

    for (uint32_t i = 0; i < bcsInfoMaskSize; i++) {
        if (bcsMask.test(i)) {
            EXPECT_NE(queue->bcsEngines[i], nullptr);
        } else {
            EXPECT_EQ(queue->bcsEngines[i], nullptr);
        }
    }
}

HWTEST2_F(CommandQueuePvcAndLaterTests, whenSelectCsrForHostPtrAllocationThenReturnProperEngine, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    DebugManager.flags.DeferCmdQBcsInitialization.set(1u);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    cl_device_id clDeviceId = static_cast<cl_device_id>(&clDevice);
    ClDeviceVector clDevices{&clDeviceId, 1u};
    cl_int retVal{};
    auto context = std::unique_ptr<Context>{Context::create<Context>(nullptr, clDevices, nullptr, nullptr, retVal)};
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto queue = std::make_unique<MockCommandQueue>(*context);
    EXPECT_EQ(0u, queue->countBcsEngines());
    queue->constructBcsEnginesForSplit();
    EXPECT_EQ(4u, queue->countBcsEngines());

    auto &csr1 = queue->selectCsrForHostPtrAllocation(true, *queue->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS1));
    EXPECT_EQ(&csr1, &queue->getGpgpuCommandStreamReceiver());

    auto &csr2 = queue->selectCsrForHostPtrAllocation(false, *queue->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS1));
    EXPECT_EQ(&csr2, queue->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS1));
}

HWTEST2_F(CommandQueuePvcAndLaterTests, whenPrepareHostPtrSurfaceForSplitThenSetTaskCountsToZero, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    DebugManager.flags.DeferCmdQBcsInitialization.set(1u);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    cl_device_id clDeviceId = static_cast<cl_device_id>(&clDevice);
    ClDeviceVector clDevices{&clDeviceId, 1u};
    cl_int retVal{};
    auto context = std::unique_ptr<Context>{Context::create<Context>(nullptr, clDevices, nullptr, nullptr, retVal)};
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto queue = std::make_unique<MockCommandQueue>(*context);
    EXPECT_EQ(0u, queue->countBcsEngines());
    queue->constructBcsEnginesForSplit();
    EXPECT_EQ(4u, queue->countBcsEngines());
    auto ptr = reinterpret_cast<void *>(0x1234);
    auto ptrSize = MemoryConstants::pageSize;
    HostPtrSurface hostPtrSurf(ptr, ptrSize);
    queue->getGpgpuCommandStreamReceiver().createAllocationForHostSurface(hostPtrSurf, false);

    queue->prepareHostPtrSurfaceForSplit(false, *hostPtrSurf.getAllocation());

    for (auto i = static_cast<uint32_t>(aub_stream::EngineType::ENGINE_BCS1); i <= static_cast<uint32_t>(aub_stream::EngineType::ENGINE_BCS8); i++) {
        auto bcs = queue->getBcsCommandStreamReceiver(static_cast<aub_stream::EngineType>(i));
        if (bcs) {
            auto contextId = bcs->getOsContext().getContextId();
            EXPECT_EQ(hostPtrSurf.getAllocation()->getTaskCount(contextId), GraphicsAllocation::objectNotUsed);
        }
    }

    queue->prepareHostPtrSurfaceForSplit(true, *hostPtrSurf.getAllocation());

    for (auto i = static_cast<uint32_t>(aub_stream::EngineType::ENGINE_BCS1); i <= static_cast<uint32_t>(aub_stream::EngineType::ENGINE_BCS8); i++) {
        auto bcs = queue->getBcsCommandStreamReceiver(static_cast<aub_stream::EngineType>(i));
        if (bcs) {
            auto contextId = bcs->getOsContext().getContextId();
            EXPECT_EQ(hostPtrSurf.getAllocation()->getTaskCount(contextId), 0u);
        }
    }

    queue->prepareHostPtrSurfaceForSplit(true, *hostPtrSurf.getAllocation());

    for (auto i = static_cast<uint32_t>(aub_stream::EngineType::ENGINE_BCS1); i <= static_cast<uint32_t>(aub_stream::EngineType::ENGINE_BCS8); i++) {
        auto bcs = queue->getBcsCommandStreamReceiver(static_cast<aub_stream::EngineType>(i));
        if (bcs) {
            auto contextId = bcs->getOsContext().getContextId();
            EXPECT_EQ(hostPtrSurf.getAllocation()->getTaskCount(contextId), 0u);
        }
    }
}

HWTEST2_F(CommandQueuePvcAndLaterTests, givenDeferCmdQBcsInitializationDisabledWhenCreateCommandQueueThenBcsIsInitialized, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    DebugManager.flags.DeferCmdQBcsInitialization.set(0u);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    cl_device_id clDeviceId = static_cast<cl_device_id>(&clDevice);
    ClDeviceVector clDevices{&clDeviceId, 1u};
    cl_int retVal{};
    auto context = std::unique_ptr<Context>{Context::create<Context>(nullptr, clDevices, nullptr, nullptr, retVal)};
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto queue = std::make_unique<MockCommandQueue>(*context);

    EXPECT_NE(0u, queue->countBcsEngines());
}

HWTEST2_F(CommandQueuePvcAndLaterTests, givenQueueWithMainBcsIsReleasedWhenNewQueueIsCreatedThenMainBcsCanBeUsedAgain, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    cl_device_id clDeviceId = static_cast<cl_device_id>(&clDevice);
    ClDeviceVector clDevices{&clDeviceId, 1u};
    cl_int retVal{};
    auto context = std::unique_ptr<Context>{Context::create<Context>(nullptr, clDevices, nullptr, nullptr, retVal)};
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto queue1 = std::make_unique<MockCommandQueue>(*context);
    auto queue2 = std::make_unique<MockCommandQueue>(*context);
    auto queue3 = std::make_unique<MockCommandQueue>(*context);
    auto queue4 = std::make_unique<MockCommandQueue>(*context);

    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, queue1->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS)->getOsContext().getEngineType());
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, queue2->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS2)->getOsContext().getEngineType());
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, queue3->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS1)->getOsContext().getEngineType());
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, queue4->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS2)->getOsContext().getEngineType());

    // Releasing main BCS. Next creation should be able to grab it
    queue1.reset();
    queue1 = std::make_unique<MockCommandQueue>(*context);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, queue1->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS)->getOsContext().getEngineType());

    // Releasing link BCS. Shouldn't change anything
    queue2.reset();
    queue2 = std::make_unique<MockCommandQueue>(*context);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, queue2->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS1)->getOsContext().getEngineType());
}

HWTEST2_F(CommandQueuePvcAndLaterTests, givenCooperativeEngineUsageHintAndCcsWhenCreatingCommandQueueThenCreateQueueWithCooperativeEngine, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    DebugManager.flags.EngineUsageHint.set(static_cast<int32_t>(EngineUsage::Cooperative));

    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
    auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto &revision : revisions) {
        auto hwRevId = hwInfoConfig.getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usRevId = hwRevId;
        if (hwRevId == CommonConstants::invalidStepping ||
            !hwInfoConfig.isCooperativeEngineSupported(hwInfo)) {
            continue;
        }

        auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
        MockContext context(pDevice.get());
        cl_queue_properties propertiesCooperativeQueue[] = {CL_QUEUE_FAMILY_INTEL, 0, CL_QUEUE_INDEX_INTEL, 0, 0};
        propertiesCooperativeQueue[1] = pDevice->getDevice().getEngineGroupIndexFromEngineGroupType(EngineGroupType::Compute);

        for (size_t i = 0; i < 4; i++) {
            propertiesCooperativeQueue[3] = i;
            auto pCommandQueue = std::make_unique<MockCommandQueueHw<FamilyType>>(&context, pDevice.get(), propertiesCooperativeQueue);
            EXPECT_EQ(aub_stream::ENGINE_CCS + i, pCommandQueue->getGpgpuEngine().osContext->getEngineType());
            EXPECT_EQ(EngineUsage::Cooperative, pCommandQueue->getGpgpuEngine().osContext->getEngineUsage());
        }
    }
}

HWTEST2_F(CommandQueuePvcAndLaterTests, givenDeferCmdQGpgpuInitializationEnabledWhenCreateCommandQueueThenGpgpuIsNullptr, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    DebugManager.flags.DeferCmdQGpgpuInitialization.set(1u);

    HardwareInfo hwInfo = *defaultHwInfo;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    cl_device_id clDeviceId = static_cast<cl_device_id>(&clDevice);
    ClDeviceVector clDevices{&clDeviceId, 1u};
    cl_int retVal{};
    auto context = std::unique_ptr<Context>{Context::create<Context>(nullptr, clDevices, nullptr, nullptr, retVal)};
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto queue = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), &clDevice, nullptr);

    EXPECT_EQ(nullptr, queue->gpgpuEngine);
}

HWTEST2_F(CommandQueuePvcAndLaterTests, givenDeferCmdQGpgpuInitializationDisabledWhenCreateCommandQueueThenGpgpuIsnotNullptr, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);
    DebugManager.flags.DeferCmdQGpgpuInitialization.set(0u);

    HardwareInfo hwInfo = *defaultHwInfo;
    MockDevice *device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0);
    MockClDevice clDevice{device};
    cl_device_id clDeviceId = static_cast<cl_device_id>(&clDevice);
    ClDeviceVector clDevices{&clDeviceId, 1u};
    cl_int retVal{};
    auto context = std::unique_ptr<Context>{Context::create<Context>(nullptr, clDevices, nullptr, nullptr, retVal)};
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto queue = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), &clDevice, nullptr);

    EXPECT_NE(nullptr, queue->gpgpuEngine);
}

struct BcsCsrSelectionCommandQueueTests : ::testing::Test {
    void SetUp() override {
        DebugManager.flags.EnableCopyEngineSelector.set(1);
        HardwareInfo hwInfo = *::defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = true;
        hwInfo.featureTable.ftrBcsInfo = maxNBitValue(bcsInfoMaskSize);

        device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo);
        clDevice = std::make_unique<MockClDevice>(device);
        context = std::make_unique<MockContext>(clDevice.get());
    }

    std::unique_ptr<MockCommandQueue> createQueueWithEngines(std::initializer_list<aub_stream::EngineType> engineTypes) {
        auto queue = createQueue(nullptr);
        queue->clearBcsEngines();
        for (auto engineType : engineTypes) {
            queue->insertBcsEngine(engineType);
        }
        EXPECT_EQ(engineTypes.size(), queue->countBcsEngines());
        return queue;
    }

    std::unique_ptr<MockCommandQueue> createQueueWithLinkBcsSelectedWithQueueFamilies(size_t linkBcsIndex) {
        cl_command_queue_properties queueProperties[5] = {};
        queueProperties[0] = CL_QUEUE_FAMILY_INTEL;
        queueProperties[1] = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::LinkedCopy);
        queueProperties[2] = CL_QUEUE_INDEX_INTEL;
        queueProperties[3] = linkBcsIndex;
        auto queue = createQueue(queueProperties);
        EXPECT_EQ(1u, queue->countBcsEngines());
        return queue;
    }

    std::unique_ptr<MockCommandQueue> createQueue(const cl_queue_properties *properties) {
        return std::make_unique<MockCommandQueue>(context.get(), clDevice.get(), properties, false);
    }

    MockDevice *device;
    std::unique_ptr<MockClDevice> clDevice;
    std::unique_ptr<MockContext> context;
    DebugManagerStateRestore restorer;
};

HWTEST2_F(BcsCsrSelectionCommandQueueTests, givenBcsSelectedWithQueueFamiliesWhenSelectingCsrThenSelectProperBcs, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    constexpr auto linkBcsType = aub_stream::ENGINE_BCS6;
    constexpr auto linkBcsIndex = 5;
    auto queue = createQueueWithLinkBcsSelectedWithQueueFamilies(linkBcsIndex);
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
}

HWTEST2_F(BcsCsrSelectionCommandQueueTests, givenBcsSelectedWithForceBcsEngineIndexWhenSelectingCsrThenSelectProperBcs, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    constexpr auto linkBcsType = aub_stream::ENGINE_BCS5;
    constexpr auto linkBcsIndex = 5;
    DebugManager.flags.ForceBcsEngineIndex.set(linkBcsIndex);
    auto queue = createQueue(nullptr);
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &selectedCsr);
    }
}

HWTEST2_F(BcsCsrSelectionCommandQueueTests, givenBcsSelectedWithQueueFamiliesAndForceBcsIndexIsUsedWhenSelectingCsrThenUseBcsFromQueueFamilies, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    constexpr auto linkBcsType = aub_stream::ENGINE_BCS6;
    constexpr auto linkBcsIndex = 5;
    DebugManager.flags.ForceBcsEngineIndex.set(2); // this should be ignored, because of queue families
    auto queue = createQueueWithLinkBcsSelectedWithQueueFamilies(linkBcsIndex);
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
}

HWTEST2_F(BcsCsrSelectionCommandQueueTests, givenOneBcsEngineInQueueWhenSelectingCsrThenTheBcs, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;

    constexpr auto linkBcsType = aub_stream::ENGINE_BCS6;
    auto queue = createQueueWithEngines({linkBcsType});
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
    {
        srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &selectedCsr);
    }
}

HWTEST2_F(BcsCsrSelectionCommandQueueTests, givenMultipleEnginesInQueueWhenSelectingCsrForLocalToLocalOperationThenSelectProperGpGpuCsr, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;
    srcGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};

    {
        auto queue = createQueueWithEngines({
            aub_stream::ENGINE_BCS,
            aub_stream::ENGINE_BCS1,
            aub_stream::ENGINE_BCS2,
            aub_stream::ENGINE_BCS3,
            aub_stream::ENGINE_BCS4,
            aub_stream::ENGINE_BCS5,
            aub_stream::ENGINE_BCS6,
            aub_stream::ENGINE_BCS7,
            aub_stream::ENGINE_BCS8,
        });
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &selectedCsr);
    }
    {
        auto queue = createQueueWithEngines({
            aub_stream::ENGINE_BCS5,
            aub_stream::ENGINE_BCS6,
            aub_stream::ENGINE_BCS7,
            aub_stream::ENGINE_BCS8,
        });
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(&queue->getGpgpuCommandStreamReceiver(), &selectedCsr);
    }
}

HWTEST2_F(BcsCsrSelectionCommandQueueTests, givenMultipleEnginesInQueueWhenSelectingCsrForNonLocalToLocalOperationThenSelectProperBcsCsr, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    BuiltinOpParams builtinOpParams{};
    MockGraphicsAllocation srcGraphicsAllocation{};
    MockGraphicsAllocation dstGraphicsAllocation{};
    MockBuffer srcMemObj{srcGraphicsAllocation};
    MockBuffer dstMemObj{dstGraphicsAllocation};
    builtinOpParams.srcMemObj = &srcMemObj;
    builtinOpParams.dstMemObj = &dstMemObj;
    srcGraphicsAllocation.memoryPool = MemoryPool::System4KBPages;
    dstGraphicsAllocation.memoryPool = MemoryPool::LocalMemory;
    CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &srcMemObj, &dstMemObj, 0u, nullptr};

    {
        auto queue = createQueueWithEngines({
            aub_stream::ENGINE_BCS,
            aub_stream::ENGINE_BCS1,
            aub_stream::ENGINE_BCS2,
            aub_stream::ENGINE_BCS3,
            aub_stream::ENGINE_BCS4,
            aub_stream::ENGINE_BCS5,
            aub_stream::ENGINE_BCS6,
            aub_stream::ENGINE_BCS7,
            aub_stream::ENGINE_BCS8,
        });
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS2), &queue->selectCsrForBuiltinOperation(args));
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS1), &queue->selectCsrForBuiltinOperation(args));
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS2), &queue->selectCsrForBuiltinOperation(args));
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS1), &queue->selectCsrForBuiltinOperation(args));
    }
}

HWTEST2_F(OoqCommandQueueHwBlitTest, givenBarrierBeforeFirstKernelWhenEnqueueNDRangeThenProgramBarrierBeforeGlobalAllocation, IsPVC) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    if (pCmdQ->getTimestampPacketContainer() == nullptr) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore{};
    DebugManager.flags.DoCpuCopyOnReadBuffer.set(0);
    DebugManager.flags.ForceCacheFlushForBcs.set(0);
    DebugManager.flags.UpdateTaskCountFromWait.set(1);
    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    MockKernel *kernel = mockKernelWithInternals.mockKernel;
    size_t offset = 0;
    size_t gws = 1;
    BufferDefaults::context = context;
    auto buffer = clUniquePtr(BufferHelper<>::create());
    char ptr[1] = {};

    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 1u, ptr, nullptr, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueBarrierWithWaitList(0, nullptr, nullptr));
    auto ccsStart = pCmdQ->getGpgpuCommandStreamReceiver().getCS().getUsed();

    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueKernel(kernel, 1, &offset, &gws, nullptr, 0, nullptr, nullptr));

    HardwareParse ccsHwParser;
    ccsHwParser.parseCommands<FamilyType>(pCmdQ->getGpgpuCommandStreamReceiver().getCS(0), ccsStart);

    const auto memFenceStateItor = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(ccsHwParser.cmdList.begin(), ccsHwParser.cmdList.end());
    const auto memFenceItor = find<MI_MEM_FENCE *>(memFenceStateItor, ccsHwParser.cmdList.end());
    EXPECT_NE(ccsHwParser.cmdList.end(), memFenceItor);
    EXPECT_NE(ccsHwParser.cmdList.end(), memFenceStateItor);
}

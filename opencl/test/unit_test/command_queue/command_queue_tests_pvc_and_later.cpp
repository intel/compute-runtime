/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

using namespace NEO;

using CommandQueuePvcAndLaterTests = ::testing::Test;

HWTEST2_F(CommandQueuePvcAndLaterTests, givenMultipleBcsEnginesWhenGetBcsCommandStreamReceiverIsCalledThenReturnProperCsrs, IsAtLeastXeHpcCore) {
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

HWTEST2_F(CommandQueuePvcAndLaterTests, givenQueueWithMainBcsIsReleasedWhenNewQueueIsCreatedThenMainBcsCanBeUsedAgain, IsAtLeastXeHpcCore) {
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
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, queue2->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS1)->getOsContext().getEngineType());
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, queue3->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS2)->getOsContext().getEngineType());
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, queue4->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS1)->getOsContext().getEngineType());

    // Releasing main BCS. Next creation should be able to grab it
    queue1.reset();
    queue1 = std::make_unique<MockCommandQueue>(*context);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, queue1->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS)->getOsContext().getEngineType());

    // Releasing link BCS. Shouldn't change anything
    queue2.reset();
    queue2 = std::make_unique<MockCommandQueue>(*context);
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, queue2->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS2)->getOsContext().getEngineType());
}

struct BcsCsrSelectionCommandQueueTests : ::testing::Test {
    void SetUp() override {
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
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
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
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(linkBcsType), &selectedCsr);
        EXPECT_EQ(linkBcsType, selectedCsr.getOsContext().getEngineType());
    }
}

HWTEST2_F(BcsCsrSelectionCommandQueueTests, givenMultipleEnginesInQueueWhenSelectingCsrForLocalToLocalOperationThenSelectProperBcsCsr, IsAtLeastXeHpcCore) {
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
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS), &selectedCsr);
        EXPECT_EQ(aub_stream::ENGINE_BCS, selectedCsr.getOsContext().getEngineType());
    }
    {
        auto queue = createQueueWithEngines({
            aub_stream::ENGINE_BCS5,
            aub_stream::ENGINE_BCS6,
            aub_stream::ENGINE_BCS7,
            aub_stream::ENGINE_BCS8,
        });
        CommandStreamReceiver &selectedCsr = queue->selectCsrForBuiltinOperation(args);
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS5), &selectedCsr);
        EXPECT_EQ(aub_stream::ENGINE_BCS5, selectedCsr.getOsContext().getEngineType());
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
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS1), &queue->selectCsrForBuiltinOperation(args));
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS2), &queue->selectCsrForBuiltinOperation(args));
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS1), &queue->selectCsrForBuiltinOperation(args));
        EXPECT_EQ(queue->getBcsCommandStreamReceiver(aub_stream::ENGINE_BCS2), &queue->selectCsrForBuiltinOperation(args));
    }
}

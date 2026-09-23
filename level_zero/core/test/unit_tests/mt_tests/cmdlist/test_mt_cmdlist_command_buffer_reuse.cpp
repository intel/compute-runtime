/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"

#include <chrono>
#include <future>

namespace L0 {
namespace ult {

struct CommandBufferReuseMtTests : CopyOffloadInOrderFixture {
    struct CountingCsr : MockCommandStreamReceiver {
        using MockCommandStreamReceiver::MockCommandStreamReceiver;

        NEO::SubmissionStatus flushTagUpdate() override {
            this->flushCount++;
            this->latestFlushedTaskCount = ++this->taskCount;
            return NEO::SubmissionStatus::success;
        }

        uint32_t flushCount = 0;
    };

    template <typename FamilyType>
    void testAppendCommandLists(bool retireBuffer, bool invalidWaitEvents, ze_result_t queueResult) {
        debugManager.flags.OverrideCopyOffloadMode.set(CopyOffloadModes::dualStream);
        debugManager.flags.EnableCommandBufferPoolAllocator.set(0);
        debugManager.flags.SetAmountOfReusableAllocations.set(0);
        debugManager.flags.DirectSubmissionFlatRingBuffer.set(0);

        auto neoDevice = device->getNEODevice();
        VariableBackup<uint32_t> maxContextCount(&NEO::MemoryManager::maxOsContextCount, NEO::MemoryManager::maxOsContextCount + 1);
        auto copyCsr = std::make_unique<CountingCsr>(*neoDevice->getExecutionEnvironment(), neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
        auto osContext = neoDevice->getMemoryManager()->createAndRegisterOsContext(copyCsr.get(), NEO::EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, NEO::EngineUsage::regular}, neoDevice->getDeviceBitfield()));
        copyCsr->setupContext(*osContext);
        copyCsr->taskCount = 5;
        copyCsr->latestFlushedTaskCount = 0;

        auto immediate = this->createImmCmdListWithOffload<FamilyType::gfxCoreFamily>();
        auto regular = this->createRegularCmdList<FamilyType::gfxCoreFamily>(false);
        ASSERT_EQ(ZE_RESULT_SUCCESS, regular->close());
        auto regularHandle = regular->toHandle();
        this->mockCmdQs.front()->executeCommandListsResult = queueResult;

        auto stream = immediate->commandContainer.getCommandStream();
        this->mockCmdQs.front()->startingCmdBuffer = stream;
        auto allocation = stream->getGraphicsAllocation();
        allocation->updateTaskCount(copyCsr->peekTaskCount(), osContext->getContextId());
        if (retireBuffer) {
            stream->getSpace(stream->getAvailableSpace());
        }

        auto copyLock = copyCsr->obtainUniqueOwnership();
        auto append = std::async(std::launch::async, [&] {
            CommandListExecutionInternalOptions options = {};
            return immediate->appendCommandLists(1, &regularHandle, nullptr, invalidWaitEvents ? 1 : 0, nullptr, options);
        });
        const auto appendStatus = append.wait_for(std::chrono::seconds(2));
        copyLock.unlock();
        EXPECT_EQ(std::future_status::ready, appendStatus);
        EXPECT_EQ(invalidWaitEvents ? ZE_RESULT_ERROR_INVALID_ARGUMENT : queueResult,
                  append.get());
        EXPECT_EQ(0u, copyCsr->flushCount);
        EXPECT_EQ(retireBuffer, allocation != immediate->commandContainer.getCommandStream()->getGraphicsAllocation());
    }
};

HWTEST2_F(CommandBufferReuseMtTests, givenCopyCsrLockedByAnotherThreadWhenAppendingCommandListsWithRolloverThenNoCopyLockOrFlushIsRequired, IsAtLeastXeCore) {
    testAppendCommandLists<FamilyType>(true, false, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandBufferReuseMtTests, givenCopyCsrLockedByAnotherThreadWhenWaitEventsAreInvalidThenNoCopyLockOrFlushIsRequired, IsAtLeastXeCore) {
    testAppendCommandLists<FamilyType>(true, true, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandBufferReuseMtTests, givenCopyCsrLockedByAnotherThreadWhenCommandListExecutionFailsThenNoCopyLockOrFlushIsRequired, IsAtLeastXeCore) {
    testAppendCommandLists<FamilyType>(true, false, ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY);
}

HWTEST2_F(CommandBufferReuseMtTests, givenCopyCsrLockedByAnotherThreadWhenAppendingWithoutRolloverThenNoCopyLockOrFlushIsRequired, IsAtLeastXeCore) {
    testAppendCommandLists<FamilyType>(false, false, ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/test/common/fixtures/aub_command_stream_receiver_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using AubCommandStreamReceiverXeHpcCoreTests = ::testing::Test;

XE_HPC_CORETEST_F(AubCommandStreamReceiverXeHpcCoreTests, givenLinkBcsEngineWhenDumpAllocationCalledThenIgnore) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    auto memoryManager = device->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, device->getDeviceBitfield()});
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor();

    MockAubManager *mockManager = new MockAubManager();
    auto gmmHelper = device->executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();
    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), *gmmHelper, false, "", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    device->executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    for (uint32_t i = aub_stream::EngineType::ENGINE_BCS1; i <= aub_stream::EngineType::ENGINE_BCS8; i++) {
        MockAubCsr<FamilyType> aubCsr("", true, *device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());

        engineDescriptor.engineTypeUsage.first = static_cast<aub_stream::EngineType>(i);

        MockOsContext osContext(0, engineDescriptor);
        aubCsr.setupContext(osContext);

        aubCsr.dumpAllocation(*gfxAllocation);

        auto mockHardwareContext = static_cast<MockHardwareContext *>(aubCsr.hardwareContextController->hardwareContexts[0].get());

        EXPECT_FALSE(mockHardwareContext->dumpSurfaceCalled);
        EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));
    }

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

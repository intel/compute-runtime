/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

namespace L0 {
namespace ult {
struct MockMemoryManagerCommandQueueSBA : public MemoryManagerMock {
    MockMemoryManagerCommandQueueSBA(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}

    uint64_t getInternalHeapBaseAddress(uint32_t rootDeviceIndex, bool useLocalMemory) override {
        getInternalHeapBaseAddressCalled++;
        getInternalHeapBaseAddressParamsPassed.push_back({rootDeviceIndex, useLocalMemory});
        return getInternalHeapBaseAddressResult;
    }

    struct GetInternalHeapBaseAddressParams {
        uint32_t rootDeviceIndex{};
        bool useLocalMemory{};
    };

    uint32_t getInternalHeapBaseAddressCalled = 0u;
    uint64_t getInternalHeapBaseAddressResult = 0u;
    StackVec<GetInternalHeapBaseAddressParams, 4> getInternalHeapBaseAddressParamsPassed{};
};

struct CommandQueueProgramSBATest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        memoryManager = new MockMemoryManagerCommandQueueSBA(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, rootDeviceIndex);
        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        device = driverHandle->devices[0];
    }
    void TearDown() override {
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    MockMemoryManagerCommandQueueSBA *memoryManager = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

using CommandQueueSBASupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandQueueProgramSBATest, whenCreatingCommandQueueThenItIsInitialized, CommandQueueSBASupport) {
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false);

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream.getSpace(alignedSize), alignedSize);

    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    const bool isaInLocalMemory = !gfxCoreHelper.useSystemMemoryPlacementForISA(neoDevice->getHardwareInfo());

    commandQueue->programStateBaseAddress(0u, true, child, true);

    EXPECT_EQ(2u, memoryManager->getInternalHeapBaseAddressCalled);
    EXPECT_EQ(rootDeviceIndex, memoryManager->getInternalHeapBaseAddressParamsPassed[0].rootDeviceIndex);
    EXPECT_EQ(rootDeviceIndex, memoryManager->getInternalHeapBaseAddressParamsPassed[1].rootDeviceIndex);

    if (isaInLocalMemory) {
        EXPECT_TRUE(memoryManager->getInternalHeapBaseAddressParamsPassed[0].useLocalMemory);
        EXPECT_TRUE(memoryManager->getInternalHeapBaseAddressParamsPassed[1].useLocalMemory);
    } else {
        EXPECT_TRUE(memoryManager->getInternalHeapBaseAddressParamsPassed[0].useLocalMemory);
        EXPECT_FALSE(memoryManager->getInternalHeapBaseAddressParamsPassed[1].useLocalMemory);
    }

    commandQueue->programStateBaseAddress(0u, false, child, true);

    EXPECT_EQ(4u, memoryManager->getInternalHeapBaseAddressCalled);
    EXPECT_EQ(rootDeviceIndex, memoryManager->getInternalHeapBaseAddressParamsPassed[2].rootDeviceIndex);
    EXPECT_EQ(rootDeviceIndex, memoryManager->getInternalHeapBaseAddressParamsPassed[3].rootDeviceIndex);

    if (isaInLocalMemory) {
        EXPECT_TRUE(memoryManager->getInternalHeapBaseAddressParamsPassed[2].useLocalMemory);
        EXPECT_FALSE(memoryManager->getInternalHeapBaseAddressParamsPassed[3].useLocalMemory);
    } else {
        EXPECT_FALSE(memoryManager->getInternalHeapBaseAddressParamsPassed[2].useLocalMemory);
        EXPECT_FALSE(memoryManager->getInternalHeapBaseAddressParamsPassed[3].useLocalMemory);
    }

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueProgramSBATest, whenProgrammingStateBaseAddressWithStatelessUncachedResourceThenCorrectMocsAreSet, CommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false);

    auto &commandStream = commandQueue->commandStream;
    auto alignedSize = commandQueue->estimateStateBaseAddressCmdSize();
    NEO::LinearStream child(commandStream.getSpace(alignedSize), alignedSize);

    auto cachedMOCSAllowed = false;
    commandQueue->programStateBaseAddress(0u, true, child, cachedMOCSAllowed);
    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandStream.getCpuBase(),
        commandStream.getUsed()));

    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto pSbaCmd = static_cast<STATE_BASE_ADDRESS *>(*itor);
    uint32_t statelessMocsIndex = pSbaCmd->getStatelessDataPortAccessMemoryObjectControlState();
    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    uint32_t expectedMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
    EXPECT_EQ(statelessMocsIndex, expectedMocs);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueProgramSBATest,
          givenBindlessModeEnabledWhenProgrammingStateBaseAddressThenBindlessBaseAddressAndSizeAreSet, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice->getMemoryManager(), neoDevice->getNumGenericSubDevices() > 1, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    NEO::MockGraphicsAllocation baseAllocation;
    bindlessHeapsHelperPtr->surfaceStateHeaps[NEO::BindlessHeapsHelper::GLOBAL_SSH].reset(new IndirectHeap(&baseAllocation, true));
    baseAllocation.setGpuBaseAddress(0x123000);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false);

    auto alignedSize = commandQueue->estimateStateBaseAddressCmdSize();
    NEO::LinearStream child(commandQueue->commandStream.getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child, true);
    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_EQ(cmdSba->getBindlessSurfaceStateBaseAddressModifyEnable(), true);

    auto globalHeapsBase = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper()->getGlobalHeapsBase();
    EXPECT_EQ(globalHeapsBase, cmdSba->getBindlessSurfaceStateBaseAddress());

    auto surfaceStateCount = StateBaseAddressHelper<FamilyType>::getMaxBindlessSurfaceStates();
    EXPECT_EQ(surfaceStateCount, cmdSba->getBindlessSurfaceStateSize());

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueProgramSBATest,
          givenBindlessModeDisabledWhenProgrammingStateBaseAddressThenBindlessBaseAddressNotPassed, CommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(0);
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice->getMemoryManager(), neoDevice->getNumGenericSubDevices() > 1, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    NEO::MockGraphicsAllocation baseAllocation;
    bindlessHeapsHelperPtr->surfaceStateHeaps[NEO::BindlessHeapsHelper::GLOBAL_SSH].reset(new IndirectHeap(&baseAllocation, true));
    baseAllocation.setGpuBaseAddress(0x123000);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false);

    auto alignedSize = commandQueue->estimateStateBaseAddressCmdSize();
    NEO::LinearStream child(commandQueue->commandStream.getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child, true);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_NE(cmdSba->getBindlessSurfaceStateBaseAddress(),
              neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper()->getGlobalHeapsBase());

    commandQueue->destroy();
}

template <bool multiTile>
struct CommandQueueCommands : DeviceFixture, ::testing::Test {
    void SetUp() override {
        DebugManager.flags.ForcePreemptionMode.set(static_cast<int>(NEO::PreemptionMode::Disabled));
        DebugManager.flags.CreateMultipleSubDevices.set(multiTile ? 2 : 1);
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }

    template <typename FamilyType>
    bool isAllocationInResidencyContainer(MockCsrHw2<FamilyType> &csr, NEO::GraphicsAllocation *graphicsAllocation) {
        for (auto alloc : csr.copyOfAllocations) {
            if (alloc == graphicsAllocation) {
                return true;
            }
        }
        return false;
    }

    const ze_command_queue_desc_t desc = {};
    DebugManagerStateRestore restore{};
    VariableBackup<bool> mockDeviceFlagBackup{&NEO::MockDevice::createSingleDevice, false};
};
using CommandQueueCommandsSingleTile = CommandQueueCommands<false>;
using CommandQueueCommandsMultiTile = CommandQueueCommands<true>;

HWTEST_F(CommandQueueCommandsSingleTile, givenCommandQueueWhenExecutingCommandListsThenHardwareContextIsProgrammedAndGlobalAllocationResident) {
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.createKernelArgsBufferAllocation();
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    auto globalFence = csr.getGlobalFenceAllocation();
    if (globalFence) {
        EXPECT_TRUE(isAllocationInResidencyContainer(csr, globalFence));
    }
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(csr.programHardwareContextCalled);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueCommandsSingleTile, givenCommandQueueWhenExecutingCommandListsThenKernelArgBufferAllocationIsResident) {
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.createKernelArgsBufferAllocation();
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    auto kernelArgsBufferAllocation = csr.getKernelArgsBufferAllocation();
    if (kernelArgsBufferAllocation) {
        EXPECT_TRUE(isAllocationInResidencyContainer(csr, kernelArgsBufferAllocation));
    }
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCommandsMultiTile, givenCommandQueueOnMultiTileWhenExecutingCommandListsThenWorkPartitionAllocationIsMadeResident, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(1);

    class MyCsrMock : public MockCsrHw2<FamilyType> {
        using MockCsrHw2<FamilyType>::MockCsrHw2;

      public:
        void makeResident(GraphicsAllocation &graphicsAllocation) override {
            if (expectedGa == &graphicsAllocation) {
                expectedGAWasMadeResident = true;
            }
        }
        GraphicsAllocation *expectedGa = nullptr;
        bool expectedGAWasMadeResident = false;
    };
    MyCsrMock csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    EXPECT_EQ(2u, csr.activePartitions);
    csr.initializeTagAllocation();
    csr.createKernelArgsBufferAllocation();
    csr.createWorkPartitionAllocation(*neoDevice);
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    auto commandListHandle = commandList->toHandle();
    auto workPartitionAllocation = csr.getWorkPartitionAllocation();
    csr.expectedGa = workPartitionAllocation;
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    EXPECT_EQ(2u, csr.activePartitionsConfig);
    ASSERT_NE(nullptr, workPartitionAllocation);
    EXPECT_TRUE(csr.expectedGAWasMadeResident);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCommandsMultiTile, givenCommandQueueOnMultiTileWhenWalkerPartitionIsDisabledThenWorkPartitionAllocationIsNotCreated, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(0);

    class MyCsrMock : public MockCsrHw2<FamilyType> {
        using MockCsrHw2<FamilyType>::MockCsrHw2;

      public:
        void makeResident(GraphicsAllocation &graphicsAllocation) override {
            if (expectedGa == &graphicsAllocation) {
                expectedGAWasMadeResident = true;
            }
        }
        GraphicsAllocation *expectedGa = nullptr;
        bool expectedGAWasMadeResident = false;
    };
    MyCsrMock csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.createWorkPartitionAllocation(*neoDevice);
    auto workPartitionAllocation = csr.getWorkPartitionAllocation();
    EXPECT_EQ(nullptr, workPartitionAllocation);
}

using CommandQueueIndirectAllocations = Test<ModuleFixture>;
HWTEST_F(CommandQueueIndirectAllocations, givenDebugModeToTreatIndirectAllocationsAsOnePackWhenIndirectAccessIsUsedThenWholePackIsMadeResident) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.MakeIndirectAllocationsResidentAsPack.set(1);
    const ze_command_queue_desc_t desc = {};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.createKernelArgsBufferAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);
    if (device->getNEODevice()->getPreemptionMode() == PreemptionMode::MidThread || device->getNEODevice()->isDebuggerActive()) {
        csr.createPreemptionAllocation();
    }

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(),
                                             &groupCount,
                                             nullptr,
                                             0,
                                             nullptr,
                                             launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                               std::end(commandList->commandContainer.getResidencyContainer()),
                               gpuAlloc);
    EXPECT_EQ(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));

    auto commandListHandle = commandList->toHandle();

    EXPECT_FALSE(gpuAlloc->isResident(csr.getOsContext().getContextId()));

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(gpuAlloc->isResident(csr.getOsContext().getContextId()));
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, gpuAlloc->getResidencyTaskCount(csr.getOsContext().getContextId()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueIndirectAllocations, givenDeviceThatSupportsSubmittingIndirectAllocationsAsPackWhenIndirectAccessIsUsedThenWholePackIsMadeResident) {
    const ze_command_queue_desc_t desc = {};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.createKernelArgsBufferAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);
    if (device->getNEODevice()->getPreemptionMode() == PreemptionMode::MidThread || device->getNEODevice()->isDebuggerActive()) {
        csr.createPreemptionAllocation();
    }

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(),
                                             &groupCount,
                                             nullptr,
                                             0,
                                             nullptr,
                                             launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                               std::end(commandList->commandContainer.getResidencyContainer()),
                               gpuAlloc);
    EXPECT_EQ(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));

    auto commandListHandle = commandList->toHandle();

    EXPECT_FALSE(gpuAlloc->isResident(csr.getOsContext().getContextId()));

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->overrideAllocateAsPackReturn = 1u;

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(gpuAlloc->isResident(csr.getOsContext().getContextId()));
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, gpuAlloc->getResidencyTaskCount(csr.getOsContext().getContextId()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueIndirectAllocations, givenDeviceThatSupportsSubmittingIndirectAllocationsAsPackWhenIndirectAccessIsUsedThenWholePackIsMadeResidentWithImmediateCommandListAndFlushTask) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily,
                                                                              device,
                                                                              &desc,
                                                                              false,
                                                                              NEO::EngineGroupType::Compute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->overrideAllocateAsPackReturn = 1u;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(),
                                             &groupCount,
                                             nullptr,
                                             0,
                                             nullptr,
                                             launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(gpuAlloc->isResident(csr.getOsContext().getContextId()));
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, gpuAlloc->getResidencyTaskCount(csr.getOsContext().getContextId()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
}

HWTEST_F(CommandQueueIndirectAllocations, givenImmediateCommandListAndFlushTaskWithIndirectAllocsAsPackDisabledThenLaunchKernelWorks) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);
    NEO::DebugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily,
                                                                              device,
                                                                              &desc,
                                                                              false,
                                                                              NEO::EngineGroupType::Compute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->overrideAllocateAsPackReturn = 1u;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(),
                                             &groupCount,
                                             nullptr,
                                             0,
                                             nullptr,
                                             launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
}

struct EngineInstancedDeviceExecuteTests : public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.EngineInstancedSubDevices.set(true);
    }

    bool createDevices(uint32_t numGenericSubDevices, uint32_t numCcs) {
        DebugManager.flags.CreateMultipleSubDevices.set(numGenericSubDevices);

        auto executionEnvironment = std::make_unique<NEO::MockExecutionEnvironment>();

        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0].get();
        auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
        hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = numCcs;
        hwInfo->featureTable.flags.ftrCCSNode = (numCcs > 0);

        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
        gfxCoreHelper.adjustDefaultEngineType(hwInfo);

        if (!multiCcsDevice(rootDeviceEnvironment, numCcs)) {
            return false;
        }
        executionEnvironment->parseAffinityMask();
        deviceFactory = std::make_unique<NEO::UltDeviceFactory>(1, numGenericSubDevices, *executionEnvironment.release());
        rootDevice = deviceFactory->rootDevices[0];
        EXPECT_NE(nullptr, rootDevice);

        return true;
    }

    bool multiCcsDevice(const NEO::RootDeviceEnvironment &rootDeviceEnvironment, uint32_t expectedNumCcs) {
        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
        auto gpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(hwInfo);

        uint32_t numCcs = 0;

        for (auto &engine : gpgpuEngines) {
            if (EngineHelpers::isCcs(engine.first) && (engine.second == EngineUsage::Regular)) {
                numCcs++;
            }
        }

        return (numCcs == expectedNumCcs);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<NEO::UltDeviceFactory> deviceFactory;
    MockDevice *rootDevice = nullptr;
};

HWTEST2_F(EngineInstancedDeviceExecuteTests, givenEngineInstancedDeviceWhenExecutingThenEnableSingleSliceDispatch, IsAtLeastXeHpCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.AllowSingleTileEngineInstancedSubDevices.set(true);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(0));
    auto defaultEngine = subDevice->getDefaultEngine();
    EXPECT_TRUE(defaultEngine.osContext->isEngineInstanced());

    std::vector<std::unique_ptr<NEO::Device>> devices;
    devices.push_back(std::unique_ptr<NEO::Device>(subDevice));

    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    auto l0Device = driverHandle->devices[0];

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    l0Device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, l0Device, csr, &desc, false, false, returnValue));
    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, l0Device, NEO::EngineGroupType::Compute, 0u, returnValue)));
    auto commandListHandle = commandList->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    GenCmdList cmdList;
    FamilyType::PARSE::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), commandQueue->commandStream.getUsed());

    auto cfeStates = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(0u, cfeStates.size());

    for (auto &cmd : cfeStates) {
        auto cfeState = reinterpret_cast<CFE_STATE *>(*cmd);
        EXPECT_TRUE(cfeState->getSingleSliceDispatchCcsMode());
    }

    commandQueue->destroy();
}

HWTEST2_F(EngineInstancedDeviceExecuteTests, givenEngineInstancedDeviceWithFabricEnumerationWhenExecutingThenEnableSingleSliceDispatch, IsAtLeastXeHpCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    constexpr uint32_t genericDevicesCount = 1;
    constexpr uint32_t ccsCount = 2;

    DebugManager.flags.AllowSingleTileEngineInstancedSubDevices.set(true);

    if (!createDevices(genericDevicesCount, ccsCount)) {
        GTEST_SKIP();
    }

    auto subDevice = static_cast<MockSubDevice *>(rootDevice->getSubDevice(0));
    auto defaultEngine = subDevice->getDefaultEngine();
    EXPECT_TRUE(defaultEngine.osContext->isEngineInstanced());

    std::vector<std::unique_ptr<NEO::Device>> devices;
    devices.push_back(std::unique_ptr<NEO::Device>(subDevice));

    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    driverHandle->initializeVertexes();

    auto l0Device = driverHandle->devices[0];

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    l0Device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, l0Device, csr, &desc, false, false, returnValue));
    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, l0Device, NEO::EngineGroupType::Compute, 0u, returnValue)));
    auto commandListHandle = commandList->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    GenCmdList cmdList;
    FamilyType::PARSE::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), commandQueue->commandStream.getUsed());

    auto cfeStates = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(0u, cfeStates.size());

    for (auto &cmd : cfeStates) {
        auto cfeState = reinterpret_cast<CFE_STATE *>(*cmd);
        EXPECT_TRUE(cfeState->getSingleSliceDispatchCcsMode());
    }

    commandQueue->destroy();
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandQueueHandleIndirectAllocs : public MockCommandQueueHw<gfxCoreFamily> {
  public:
    using typename MockCommandQueueHw<gfxCoreFamily>::CommandListExecutionContext;
    using MockCommandQueueHw<gfxCoreFamily>::executeCommandListsRegular;
    MockCommandQueueHandleIndirectAllocs(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : MockCommandQueueHw<gfxCoreFamily>(device, csr, desc) {}
    void handleIndirectAllocationResidency(UnifiedMemoryControls unifiedMemoryControls, std::unique_lock<std::mutex> &lockForIndirect, bool performMigration) override {
        handleIndirectAllocationResidencyCalledTimes++;
        MockCommandQueueHw<gfxCoreFamily>::handleIndirectAllocationResidency(unifiedMemoryControls, lockForIndirect, performMigration);
    }
    void makeResidentAndMigrate(bool performMigration, const NEO::ResidencyContainer &residencyContainer) override {
        makeResidentAndMigrateCalledTimes++;
    }
    uint32_t handleIndirectAllocationResidencyCalledTimes = 0;
    uint32_t makeResidentAndMigrateCalledTimes = 0;
};

HWTEST2_F(CommandQueueIndirectAllocations, givenCtxWithIndirectAccessWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessCalled, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHandleIndirectAllocs<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);
    auto ctx = typename MockCommandQueueHandleIndirectAllocs<gfxCoreFamily>::CommandListExecutionContext{nullptr,
                                                                                                         0,
                                                                                                         csr->getPreemptionMode(),
                                                                                                         device,
                                                                                                         false,
                                                                                                         csr->isProgramActivePartitionConfigRequired(),
                                                                                                         false};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    ctx.hasIndirectAccess = true;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    auto cmdListHandle = commandList.get()->toHandle();
    commandQueue->executeCommandListsRegular(ctx, 1, &cmdListHandle, nullptr);
    EXPECT_EQ(commandQueue->handleIndirectAllocationResidencyCalledTimes, 1u);
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueIndirectAllocations, givenCtxWitNohIndirectAccessWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessNotCalled, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHandleIndirectAllocs<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);
    auto ctx = typename MockCommandQueueHandleIndirectAllocs<gfxCoreFamily>::CommandListExecutionContext{nullptr,
                                                                                                         0,
                                                                                                         csr->getPreemptionMode(),
                                                                                                         device,
                                                                                                         false,
                                                                                                         csr->isProgramActivePartitionConfigRequired(),
                                                                                                         false};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    ctx.hasIndirectAccess = false;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    auto cmdListHandle = commandList.get()->toHandle();
    commandQueue->executeCommandListsRegular(ctx, 1, &cmdListHandle, nullptr);
    EXPECT_EQ(commandQueue->handleIndirectAllocationResidencyCalledTimes, 0u);
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueIndirectAllocations, givenCommandQueueWhenHandleIndirectAllocationResidencyCalledAndSubmitPackDiasabledThenMakeResidentAndMigrateCalled, IsAtLeastSkl) {
    DebugManagerStateRestore restore;
    DebugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);

    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHandleIndirectAllocs<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);
    auto ctx = typename MockCommandQueueHandleIndirectAllocs<gfxCoreFamily>::CommandListExecutionContext{nullptr,
                                                                                                         0,
                                                                                                         csr->getPreemptionMode(),
                                                                                                         device,
                                                                                                         false,
                                                                                                         csr->isProgramActivePartitionConfigRequired(),
                                                                                                         false};
    std::unique_lock<std::mutex> lock;

    commandQueue->handleIndirectAllocationResidency({true, true, true}, lock, false);
    EXPECT_EQ(commandQueue->makeResidentAndMigrateCalledTimes, 1u);
    commandQueue->destroy();
}

using CommandQueueTest = Test<DeviceFixture>;

HWTEST_F(CommandQueueTest, givenCommandQueueWhenMakeResidentAndMigrateWithEmptyResidencyContainerThenMakeResidentWasNotCalled) {
    MockCommandStreamReceiver csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ResidencyContainer container;
    commandQueue->makeResidentAndMigrate(false, container);
    EXPECT_EQ(csr.makeResidentCalledTimes, 0u);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueTest, givenCommandQueueWhenMakeResidentAndMigrateWithTwoAllocsInContainerThenMakeResidentCalledTwice) {
    MockCommandStreamReceiver csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ResidencyContainer container;
    MockGraphicsAllocation mockGA1;
    MockGraphicsAllocation mockGA2;
    container.push_back(&mockGA1);
    container.push_back(&mockGA2);
    commandQueue->makeResidentAndMigrate(false, container);
    EXPECT_EQ(csr.makeResidentCalledTimes, 2u);
    commandQueue->destroy();
}
HWTEST_F(CommandQueueTest, givenCommandQueueWhenPerformMigrationIsFalseThenTransferToGpuWasNotCalled) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockCommandStreamReceiver csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ResidencyContainer container;
    MockGraphicsAllocation mockGA;
    container.push_back(&mockGA);
    commandQueue->makeResidentAndMigrate(false, container);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 0);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueTest, givenCommandQueueWhenPerformMigrationIsTrueAndAllocationTypeIsSvmGpuThenTransferToGpuWasCalled) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockCommandStreamReceiver csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ResidencyContainer container;
    MockGraphicsAllocation mockGA;
    mockGA.allocationType = NEO::AllocationType::SVM_GPU;
    container.push_back(&mockGA);
    commandQueue->makeResidentAndMigrate(true, container);
    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalled, 1);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueTest, givenCommandQueueWhenPerformMigrationIsTrueAndAllocationTypeIsSvmCpuThenTransferToGpuWasCalled) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockCommandStreamReceiver csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ResidencyContainer container;
    MockGraphicsAllocation mockGA;
    mockGA.allocationType = NEO::AllocationType::SVM_CPU;
    container.push_back(&mockGA);
    commandQueue->makeResidentAndMigrate(true, container);
    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalled, 1);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueTest, givenCommandQueueWhenPerformMigrationIsTrueAndAllocationTypeIsNoSvmTypeThenTransferToGpuWasNotCalled) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(neoDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockCommandStreamReceiver csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ResidencyContainer container;
    MockGraphicsAllocation mockGA;
    mockGA.allocationType = NEO::AllocationType::TAG_BUFFER;
    container.push_back(&mockGA);
    commandQueue->makeResidentAndMigrate(true, container);
    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalled, 0);
    commandQueue->destroy();
}

} // namespace ult
} // namespace L0

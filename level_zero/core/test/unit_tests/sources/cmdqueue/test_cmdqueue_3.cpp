/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
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
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
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
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    auto &hwHelper = HwHelper::get(neoDevice->getHardwareInfo().platform.eRenderCoreFamily);
    const bool isaInLocalMemory = !hwHelper.useSystemMemoryPlacementForISA(neoDevice->getHardwareInfo());

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

HWTEST2_F(CommandQueueProgramSBATest,
          whenProgrammingStateBaseAddressWithcontainsStatelessUncachedResourceThenCorrectMocsAreSet, CommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false);

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child, true);
    auto pSbaCmd = static_cast<STATE_BASE_ADDRESS *>(commandQueue->commandStream->getSpace(sizeof(STATE_BASE_ADDRESS)));
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

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child, true);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_EQ(cmdSba->getBindlessSurfaceStateBaseAddressModifyEnable(), true);
    EXPECT_EQ(cmdSba->getBindlessSurfaceStateBaseAddress(), neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper()->getGlobalHeapsBase());

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

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child, true);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_NE(cmdSba->getBindlessSurfaceStateBaseAddress(), neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper()->getGlobalHeapsBase());

    commandQueue->destroy();
}

template <bool multiTile>
struct CommandQueueCommands : DeviceFixture, ::testing::Test {
    void SetUp() override {
        DebugManager.flags.ForcePreemptionMode.set(static_cast<int>(NEO::PreemptionMode::Disabled));
        DebugManager.flags.CreateMultipleSubDevices.set(multiTile ? 2 : 1);
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
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
HWTEST_F(CommandQueueIndirectAllocations, givenCommandQueueWhenExecutingCommandListsThenExpectedIndirectAllocationsAddedToResidencyContainer) {
    const ze_command_queue_desc_t desc = {};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
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
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                          std::end(commandList->commandContainer.getResidencyContainer()),
                          gpuAlloc);
    EXPECT_NE(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueIndirectAllocations, givenDebugModeToTreatIndirectAllocationsAsOnePackWhenIndirectAccessIsUsedThenWholePackIsMadeResident) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.MakeIndirectAllocationsResidentAsPack.set(1);
    const ze_command_queue_desc_t desc = {};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
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

        auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);

        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = numCcs;
        hwInfo->featureTable.flags.ftrCCSNode = (numCcs > 0);
        HwHelper::get(hwInfo->platform.eRenderCoreFamily).adjustDefaultEngineType(hwInfo);

        if (!multiCcsDevice(*hwInfo, numCcs)) {
            return false;
        }
        executionEnvironment->parseAffinityMask();
        deviceFactory = std::make_unique<NEO::UltDeviceFactory>(1, numGenericSubDevices, *executionEnvironment.release());
        rootDevice = deviceFactory->rootDevices[0];
        EXPECT_NE(nullptr, rootDevice);

        return true;
    }

    bool multiCcsDevice(const HardwareInfo &hwInfo, uint32_t expectedNumCcs) {
        auto gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo);

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
    FamilyType::PARSE::parseCommandBuffer(cmdList, commandQueue->commandStream->getCpuBase(), commandQueue->commandStream->getUsed());

    auto cfeStates = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(0u, cfeStates.size());

    for (auto &cmd : cfeStates) {
        auto cfeState = reinterpret_cast<CFE_STATE *>(*cmd);
        EXPECT_TRUE(cfeState->getSingleSliceDispatchCcsMode());
    }

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/common/ult_helpers_l0.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue_handle_indirect_allocs.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
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
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
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

using CommandQueueSBASupport = IsGen12LP;
HWTEST2_F(CommandQueueProgramSBATest, whenCreatingCommandQueueThenItIsInitialized, CommandQueueSBASupport) {
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false, false);

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream.getSpace(alignedSize), alignedSize);

    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    const bool isaInLocalMemory = !gfxCoreHelper.useSystemMemoryPlacementForISA(neoDevice->getHardwareInfo());

    commandQueue->programStateBaseAddress(0u, true, child, true, nullptr);

    EXPECT_EQ(1u, memoryManager->getInternalHeapBaseAddressCalled);
    EXPECT_EQ(rootDeviceIndex, memoryManager->getInternalHeapBaseAddressParamsPassed[0].rootDeviceIndex);
    EXPECT_EQ(isaInLocalMemory, memoryManager->getInternalHeapBaseAddressParamsPassed[0].useLocalMemory);

    commandQueue->programStateBaseAddress(0u, false, child, true, nullptr);

    EXPECT_EQ(2u, memoryManager->getInternalHeapBaseAddressCalled);
    EXPECT_EQ(rootDeviceIndex, memoryManager->getInternalHeapBaseAddressParamsPassed[1].rootDeviceIndex);
    EXPECT_EQ(isaInLocalMemory, memoryManager->getInternalHeapBaseAddressParamsPassed[1].useLocalMemory);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueProgramSBATest, whenProgrammingStateBaseAddressWithStatelessUncachedResourceThenCorrectMocsAreSet, CommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false, false);

    auto &commandStream = commandQueue->commandStream;
    auto alignedSize = commandQueue->estimateStateBaseAddressCmdSize();
    NEO::LinearStream child(commandStream.getSpace(alignedSize), alignedSize);

    auto cachedMOCSAllowed = false;
    commandQueue->programStateBaseAddress(0u, true, child, cachedMOCSAllowed, nullptr);
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
    uint32_t expectedMocs = gmmHelper->getUncachedMOCS();
    EXPECT_EQ(statelessMocsIndex, expectedMocs);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueProgramSBATest,
          givenGlobalSshWhenProgrammingStateBaseAddressThenBindlessBaseAddressAndSizeAreSet, CommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice, neoDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    NEO::MockGraphicsAllocation baseAllocation;
    bindlessHeapsHelperPtr->surfaceStateHeaps[NEO::BindlessHeapsHelper::globalSsh].reset(new IndirectHeap(&baseAllocation, true));
    baseAllocation.setGpuBaseAddress(0x123000);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false, false);

    auto alignedSize = commandQueue->estimateStateBaseAddressCmdSize();
    NEO::LinearStream child(commandQueue->commandStream.getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child, true, nullptr);
    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), usedSpaceAfter));

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
          givenGlobalSshAndDshWhenProgrammingStateBaseAddressThenBindlessBaseAddressAndSizeAreSet, CommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice, neoDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    bindlessHeapsHelperPtr->globalBindlessDsh = true;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    NEO::MockGraphicsAllocation baseAllocation;
    bindlessHeapsHelperPtr->surfaceStateHeaps[NEO::BindlessHeapsHelper::globalSsh].reset(new IndirectHeap(&baseAllocation, true));
    baseAllocation.setGpuBaseAddress(0x123000);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false, false);

    auto alignedSize = commandQueue->estimateStateBaseAddressCmdSize();
    NEO::LinearStream child(commandQueue->commandStream.getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child, true, nullptr);
    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_EQ(cmdSba->getBindlessSurfaceStateBaseAddressModifyEnable(), true);

    auto globalHeapsBase = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper()->getGlobalHeapsBase();
    EXPECT_EQ(globalHeapsBase, cmdSba->getBindlessSurfaceStateBaseAddress());

    auto surfaceStateCount = StateBaseAddressHelper<FamilyType>::getMaxBindlessSurfaceStates();
    EXPECT_EQ(surfaceStateCount, cmdSba->getBindlessSurfaceStateSize());

    EXPECT_EQ(globalHeapsBase, cmdSba->getDynamicStateBaseAddress());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, cmdSba->getDynamicStateBufferSize());
    EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueProgramSBATest,
          givenGlobalBindlessSshWhenProgrammingStateBaseAddressThenBindlessBaseAddressIsSet, CommandQueueSBASupport) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice, neoDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    NEO::MockGraphicsAllocation baseAllocation;
    bindlessHeapsHelperPtr->surfaceStateHeaps[NEO::BindlessHeapsHelper::globalSsh].reset(new IndirectHeap(&baseAllocation, true));
    baseAllocation.setGpuBaseAddress(0x123000);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false, false, false);

    auto alignedSize = commandQueue->estimateStateBaseAddressCmdSize();
    NEO::LinearStream child(commandQueue->commandStream.getSpace(alignedSize), alignedSize);

    commandQueue->programStateBaseAddress(0u, true, child, true, nullptr);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_EQ(cmdSba->getBindlessSurfaceStateBaseAddress(),
              neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->getBindlessHeapsHelper()->getGlobalHeapsBase());

    commandQueue->destroy();
}

template <bool multiTile>
struct CommandQueueCommands : DeviceFixture, ::testing::Test {
    void SetUp() override {
        debugManager.flags.ForcePreemptionMode.set(static_cast<int>(NEO::PreemptionMode::Disabled));
        debugManager.flags.CreateMultipleSubDevices.set(multiTile ? 2 : 1);
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
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    auto globalFence = csr.getGlobalFenceAllocation();
    if (globalFence) {
        EXPECT_TRUE(isAllocationInResidencyContainer(csr, globalFence));
    }
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(csr.programHardwareContextCalled);
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCommandsMultiTile, givenCommandQueueOnMultiTileWhenExecutingCommandListsThenWorkPartitionAllocationIsMadeResident, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWalkerPartition.set(1);

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
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList->close();

    auto commandListHandle = commandList->toHandle();
    auto workPartitionAllocation = csr.getWorkPartitionAllocation();
    csr.expectedGa = workPartitionAllocation;
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    EXPECT_EQ(2u, csr.activePartitionsConfig);
    ASSERT_NE(nullptr, workPartitionAllocation);
    EXPECT_TRUE(csr.expectedGAWasMadeResident);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCommandsMultiTile, givenCommandQueueOnMultiTileWhenWalkerPartitionIsDisabledThenWorkPartitionAllocationIsNotCreated, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableWalkerPartition.set(0);

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
    debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(1);
    const ze_command_queue_desc_t desc = {};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);
    if (device->getNEODevice()->getPreemptionMode() == PreemptionMode::MidThread) {
        csr.createPreemptionAllocation();
    }

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    kernel->kernelHasIndirectAccess = true;

    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(),
                                             groupCount,
                                             nullptr, 0, nullptr,
                                             launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto itorEvent = std::find(std::begin(commandList->getCmdContainer().getResidencyContainer()),
                               std::end(commandList->getCmdContainer().getResidencyContainer()),
                               gpuAlloc);
    EXPECT_EQ(itorEvent, std::end(commandList->getCmdContainer().getResidencyContainer()));
    commandList->close();
    auto commandListHandle = commandList->toHandle();

    EXPECT_FALSE(gpuAlloc->isResident(csr.getOsContext().getContextId()));

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(gpuAlloc->isResident(csr.getOsContext().getContextId()));
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, gpuAlloc->getResidencyTaskCount(csr.getOsContext().getContextId()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
    commandQueue->destroy();
    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
}

HWTEST_F(CommandQueueIndirectAllocations, givenDeviceThatSupportsSubmittingIndirectAllocationsAsPackWhenIndirectAccessIsUsedThenWholePackIsMadeResident) {
    const ze_command_queue_desc_t desc = {};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);
    if (device->getNEODevice()->getPreemptionMode() == PreemptionMode::MidThread) {
        csr.createPreemptionAllocation();
    }

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    kernel->kernelHasIndirectAccess = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(),
                                             groupCount,
                                             nullptr, 0, nullptr,
                                             launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto itorEvent = std::find(std::begin(commandList->getCmdContainer().getResidencyContainer()),
                               std::end(commandList->getCmdContainer().getResidencyContainer()),
                               gpuAlloc);
    EXPECT_EQ(itorEvent, std::end(commandList->getCmdContainer().getResidencyContainer()));
    commandList->close();
    auto commandListHandle = commandList->toHandle();

    EXPECT_FALSE(gpuAlloc->isResident(csr.getOsContext().getContextId()));

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->overrideAllocateAsPackReturn = 1u;

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(gpuAlloc->isResident(csr.getOsContext().getContextId()));
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, gpuAlloc->getResidencyTaskCount(csr.getOsContext().getContextId()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
    commandQueue->destroy();
    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
}

HWTEST_F(CommandQueueIndirectAllocations, givenDeviceThatSupportsSubmittingIndirectAllocationsAsPackWhenIndirectAccessIsUsedThenWholePackIsMadeResidentWithImmediateCommandListAndFlushTask) {
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
                                                                              NEO::EngineGroupType::compute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<L0::ult::CommandList *>(commandList.get());

    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    kernel->kernelHasIndirectAccess = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->overrideAllocateAsPackReturn = 1u;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(),
                                             groupCount,
                                             nullptr, 0, nullptr,
                                             launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(gpuAlloc->isResident(csr.getOsContext().getContextId()));
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, gpuAlloc->getResidencyTaskCount(csr.getOsContext().getContextId()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
}

HWTEST_F(CommandQueueIndirectAllocations,
         givenDeviceThatSupportsSubmittingIndirectAllocationsAsPackWhenIndirectAccessIsUsedWhenExecutingRegularOnImmediateThenWholePackIsMadeResidentWithRegularCommandList) {

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    std::unique_ptr<L0::CommandList> commandListImmediate(CommandList::createImmediate(productFamily,
                                                                                       device,
                                                                                       &desc,
                                                                                       false,
                                                                                       NEO::EngineGroupType::compute,
                                                                                       returnValue));
    ASSERT_NE(nullptr, commandListImmediate);

    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily,
                                                                            device,
                                                                            NEO::EngineGroupType::compute,
                                                                            0u,
                                                                            returnValue, false));
    ASSERT_NE(nullptr, commandListRegular);

    void *deviceAlloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    kernel->kernelHasIndirectAccess = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    static_cast<MockMemoryManager *>(driverHandle->getMemoryManager())->overrideAllocateAsPackReturn = 1u;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandListRegular->appendLaunchKernel(kernel->toHandle(),
                                                    groupCount,
                                                    nullptr, 0, nullptr,
                                                    launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    commandListRegular->close();

    auto cmdListHandle = commandListRegular->toHandle();
    result = commandListImmediate->appendCommandLists(1, &cmdListHandle, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(gpuAlloc->isResident(csr.getOsContext().getContextId()));
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, gpuAlloc->getResidencyTaskCount(csr.getOsContext().getContextId()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
}

HWTEST_F(CommandQueueIndirectAllocations, givenImmediateCommandListAndFlushTaskWithIndirectAllocsAsPackDisabledThenLaunchKernelWorks) {
    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);

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
                                                                              NEO::EngineGroupType::compute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

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
                                             groupCount,
                                             nullptr, 0, nullptr,
                                             launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
}

struct SingleSliceDispatchSupportMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        using GfxProduct = typename HwMapper<productFamily>::GfxProduct;
        return GfxProduct::FrontEndStateSupport::singleSliceDispatchCcsMode;
    }
};

HWTEST_F(CommandQueueIndirectAllocations, givenCtxWithIndirectAccessWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessCalled) {
    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHandleIndirectAllocs<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto cmdListHandle = commandList->toHandle();
    commandList->close();

    auto ctx = typename MockCommandQueueHandleIndirectAllocs<FamilyType::gfxCoreFamily>::CommandListExecutionContext{&cmdListHandle,
                                                                                                                     1,
                                                                                                                     csr->getPreemptionMode(),
                                                                                                                     device,
                                                                                                                     csr->getScratchSpaceController(),
                                                                                                                     csr->getGlobalStatelessHeapAllocation(),
                                                                                                                     false,
                                                                                                                     csr->isProgramActivePartitionConfigRequired(),
                                                                                                                     false,
                                                                                                                     false};

    ctx.hasIndirectAccess = true;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    commandQueue->executeCommandListsRegular(ctx, 1, &cmdListHandle, nullptr, nullptr);
    EXPECT_EQ(commandQueue->handleIndirectAllocationResidencyCalledTimes, 1u);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueIndirectAllocations, givenCtxWitNohIndirectAccessWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessNotCalled) {
    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHandleIndirectAllocs<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList->close();
    auto cmdListHandle = commandList.get()->toHandle();

    auto ctx = typename MockCommandQueueHandleIndirectAllocs<FamilyType::gfxCoreFamily>::CommandListExecutionContext{&cmdListHandle,
                                                                                                                     1,
                                                                                                                     csr->getPreemptionMode(),
                                                                                                                     device,
                                                                                                                     csr->getScratchSpaceController(),
                                                                                                                     csr->getGlobalStatelessHeapAllocation(),
                                                                                                                     false,
                                                                                                                     csr->isProgramActivePartitionConfigRequired(),
                                                                                                                     false,
                                                                                                                     false};

    ctx.hasIndirectAccess = false;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    commandQueue->executeCommandListsRegular(ctx, 1, &cmdListHandle, nullptr, nullptr);
    EXPECT_EQ(commandQueue->handleIndirectAllocationResidencyCalledTimes, 0u);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueIndirectAllocations, givenCommandQueueWhenHandleIndirectAllocationResidencyCalledAndSubmitPackDiasabledThenMakeResidentAndMigrateCalled) {
    DebugManagerStateRestore restore;
    debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);

    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHandleIndirectAllocs<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
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
                                                          false,
                                                          returnValue);
    ResidencyContainer container;
    MockGraphicsAllocation mockGA;
    mockGA.allocationType = NEO::AllocationType::svmGpu;
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
                                                          false,
                                                          returnValue);
    ResidencyContainer container;
    MockGraphicsAllocation mockGA;
    mockGA.allocationType = NEO::AllocationType::svmCpu;
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
                                                          false,
                                                          returnValue);
    ResidencyContainer container;
    MockGraphicsAllocation mockGA;
    mockGA.allocationType = NEO::AllocationType::tagBuffer;
    container.push_back(&mockGA);
    commandQueue->makeResidentAndMigrate(true, container);
    EXPECT_EQ(mockPageFaultManager->moveAllocationToGpuDomainCalled, 0);
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueTest, givenBindlessEnabledWhenEstimateStateBaseAddressCmdSizeCalledThenZeroSizeIsReturned, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseBindlessMode.set(1);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr.get(), &desc);
    auto size = commandQueue->estimateStateBaseAddressCmdSize();
    auto expectedSize = 0u;
    EXPECT_EQ(size, expectedSize);
}

HWTEST2_F(CommandQueueTest, givenBindlessDisabledWhenEstimateStateBaseAddressCmdSizeCalledThenZeroReturned, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseBindlessMode.set(0);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr.get(), &desc);
    auto size = commandQueue->estimateStateBaseAddressCmdSize();
    auto expectedSize = 0u;
    EXPECT_EQ(size, expectedSize);
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t estimateAllCommmandLists(MockCommandQueueHw<gfxCoreFamily> *commandQueue, NEO::CommandStreamReceiver *csr, ze_command_list_handle_t *commandListHandles, size_t handlesSize, bool frontEndStateDirty) {
    size_t estimatedSize = 0;
    auto csrStateCopy = csr->getStreamProperties();
    bool feDirty = false;
    bool feRetPoint = false;
    NEO::StreamProperties dummyProperties{};
    for (size_t i = 0; i < handlesSize; i++) {
        auto cmdListPtr = CommandList::fromHandle(commandListHandles[i]);
        auto requiredState = cmdListPtr->getRequiredStreamState();
        auto finalState = cmdListPtr->getFinalStreamState();
        estimatedSize += commandQueue->estimateFrontEndCmdSizeForMultipleCommandLists(frontEndStateDirty, cmdListPtr, csrStateCopy, requiredState, finalState, dummyProperties, feDirty, feRetPoint);
    }
    return estimatedSize;
}

HWTEST2_F(CommandQueueTest, whenExecuteCommandListsIsCalledThenCorrectSizeOfFrontEndCmdsIsCalculatedAndCorrectStateIsSet, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;

    debugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ASSERT_NE(nullptr, csr);

    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>{device, csr, &desc};
    commandQueue->initialize(false, false, false);

    Mock<::L0::KernelImp> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::KernelImp> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
    cooperativeKernel.setGroupSize(1, 1, 1);

    ze_group_count_t threadGroupDimensions{1, 1, 1};
    auto commandListA = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListA->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);

    CmdListKernelLaunchParams launchParams = {};
    commandListA->appendLaunchKernelWithParams(&defaultKernel, threadGroupDimensions, nullptr, launchParams);
    commandListA->close();

    auto commandListBB = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListBB->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);

    launchParams.isCooperative = true;
    commandListBB->appendLaunchKernelWithParams(&cooperativeKernel, threadGroupDimensions, nullptr, launchParams);
    commandListBB->appendLaunchKernelWithParams(&cooperativeKernel, threadGroupDimensions, nullptr, launchParams);
    commandListBB->close();

    auto commandListAB = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListAB->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);

    launchParams.isCooperative = false;
    commandListAB->appendLaunchKernelWithParams(&defaultKernel, threadGroupDimensions, nullptr, launchParams);

    launchParams.isCooperative = true;
    commandListAB->appendLaunchKernelWithParams(&cooperativeKernel, threadGroupDimensions, nullptr, launchParams);
    commandListAB->close();

    auto commandListBA = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListBA->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);
    commandListBA->appendLaunchKernelWithParams(&cooperativeKernel, threadGroupDimensions, nullptr, launchParams);

    launchParams.isCooperative = false;
    commandListBA->appendLaunchKernelWithParams(&defaultKernel, threadGroupDimensions, nullptr, launchParams);
    commandListBA->close();

    auto commandListBAB = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListBAB->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);

    launchParams.isCooperative = true;
    commandListBAB->appendLaunchKernelWithParams(&cooperativeKernel, threadGroupDimensions, nullptr, launchParams);

    launchParams.isCooperative = false;
    commandListBAB->appendLaunchKernelWithParams(&defaultKernel, threadGroupDimensions, nullptr, launchParams);

    launchParams.isCooperative = true;
    commandListBAB->appendLaunchKernelWithParams(&cooperativeKernel, threadGroupDimensions, nullptr, launchParams);
    commandListBAB->close();

    auto commandListAAB = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListAAB->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);

    launchParams.isCooperative = false;
    commandListAAB->appendLaunchKernelWithParams(&defaultKernel, threadGroupDimensions, nullptr, launchParams);
    commandListAAB->appendLaunchKernelWithParams(&defaultKernel, threadGroupDimensions, nullptr, launchParams);

    launchParams.isCooperative = true;
    commandListAAB->appendLaunchKernelWithParams(&cooperativeKernel, threadGroupDimensions, nullptr, launchParams);
    commandListAAB->close();

    auto commandListEmpty = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListEmpty->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);
    commandListEmpty->close();

    size_t singleFrontEndCmdSize = commandQueue->estimateFrontEndCmdSize();

    NEO::FrontEndPropertiesSupport fePropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, device->getHwInfo());
    uint32_t expectedSingleFrontEndSizeNumber = fePropertiesSupport.computeDispatchAllWalker ? 1 : 0;
    expectedSingleFrontEndSizeNumber = fePropertiesSupport.disableEuFusion ? 1 : expectedSingleFrontEndSizeNumber;

    EXPECT_EQ(-1, csr->getStreamProperties().frontEndState.computeDispatchAllWalkerEnable.value);
    {
        ze_command_list_handle_t commandLists[] = {commandListA->toHandle(), commandListAB->toHandle(), commandListBA->toHandle(), commandListA->toHandle()};
        size_t commandListSize = sizeof(commandLists) / sizeof(ze_command_list_handle_t);

        size_t calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, false);
        EXPECT_EQ(expectedSingleFrontEndSizeNumber * singleFrontEndCmdSize, calculatedSize);

        calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, true);
        EXPECT_EQ(1 * singleFrontEndCmdSize, calculatedSize);
        commandQueue->executeCommandLists(4, commandLists, nullptr, false, nullptr, nullptr);
    }
    int32_t expectedComputeDispatchAllWalkerEnable = fePropertiesSupport.computeDispatchAllWalker ? 0 : -1;
    expectedSingleFrontEndSizeNumber = fePropertiesSupport.computeDispatchAllWalker ? 1 : 0;
    EXPECT_EQ(expectedComputeDispatchAllWalkerEnable, csr->getStreamProperties().frontEndState.computeDispatchAllWalkerEnable.value);
    {
        ze_command_list_handle_t commandLists[] = {commandListAAB->toHandle(), commandListBAB->toHandle(), commandListA->toHandle()};
        size_t commandListSize = sizeof(commandLists) / sizeof(ze_command_list_handle_t);

        size_t calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, false);
        EXPECT_EQ(expectedSingleFrontEndSizeNumber * singleFrontEndCmdSize, calculatedSize);

        calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, true);
        EXPECT_EQ((expectedSingleFrontEndSizeNumber + 1) * singleFrontEndCmdSize, calculatedSize);
        commandQueue->executeCommandLists(3, commandLists, nullptr, false, nullptr, nullptr);
    }
    EXPECT_EQ(expectedComputeDispatchAllWalkerEnable, csr->getStreamProperties().frontEndState.computeDispatchAllWalkerEnable.value);
    {
        ze_command_list_handle_t commandLists[] = {commandListEmpty->toHandle(), commandListA->toHandle()};
        size_t commandListSize = sizeof(commandLists) / sizeof(ze_command_list_handle_t);

        size_t calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, false);
        EXPECT_EQ(0 * singleFrontEndCmdSize, calculatedSize);

        calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, true);
        EXPECT_EQ(1 * singleFrontEndCmdSize, calculatedSize);
    }
    {
        ze_command_list_handle_t commandLists[] = {commandListBB->toHandle()};
        size_t commandListSize = sizeof(commandLists) / sizeof(ze_command_list_handle_t);

        size_t calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, false);
        EXPECT_EQ(expectedSingleFrontEndSizeNumber * singleFrontEndCmdSize, calculatedSize);

        calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, true);
        EXPECT_EQ(1 * singleFrontEndCmdSize, calculatedSize);
        commandQueue->executeCommandLists(1, commandLists, nullptr, false, nullptr, nullptr);
    }
    expectedComputeDispatchAllWalkerEnable = fePropertiesSupport.computeDispatchAllWalker ? 1 : -1;
    EXPECT_EQ(expectedComputeDispatchAllWalkerEnable, csr->getStreamProperties().frontEndState.computeDispatchAllWalkerEnable.value);
    {
        ze_command_list_handle_t commandLists[] = {commandListA->toHandle()};
        size_t commandListSize = sizeof(commandLists) / sizeof(ze_command_list_handle_t);

        size_t calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, false);
        EXPECT_EQ(expectedSingleFrontEndSizeNumber * singleFrontEndCmdSize, calculatedSize);

        calculatedSize = estimateAllCommmandLists<FamilyType::gfxCoreFamily>(commandQueue, csr, commandLists, commandListSize, true);
        EXPECT_EQ(1 * singleFrontEndCmdSize, calculatedSize);
        commandQueue->executeCommandLists(1, commandLists, nullptr, false, nullptr, nullptr);
    }
    expectedComputeDispatchAllWalkerEnable = fePropertiesSupport.computeDispatchAllWalker ? 0 : -1;
    EXPECT_EQ(expectedComputeDispatchAllWalkerEnable, csr->getStreamProperties().frontEndState.computeDispatchAllWalkerEnable.value);
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueTest, givenRegularKernelScheduledAsCooperativeWhenExecuteCommandListsIsCalledThenComputeDispatchAllWalkerEnableIsSet, IsAtLeastXeCore) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ASSERT_NE(nullptr, csr);

    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>{device, csr, &desc};
    commandQueue->initialize(false, false, false);

    Mock<::L0::KernelImp> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    ze_group_count_t threadGroupDimensions{1, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = true;
    commandList->appendLaunchKernelWithParams(&defaultKernel, threadGroupDimensions, nullptr, launchParams);
    commandList->close();

    EXPECT_EQ(-1, csr->getStreamProperties().frontEndState.computeDispatchAllWalkerEnable.value);

    ze_command_list_handle_t commandLists[] = {commandList->toHandle()};
    commandQueue->executeCommandLists(1, commandLists, nullptr, false, nullptr, nullptr);
    NEO::FrontEndPropertiesSupport fePropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, device->getHwInfo());
    int32_t expectedComputeDispatchAllWalkerEnableNumber = fePropertiesSupport.computeDispatchAllWalker ? 1 : -1;
    EXPECT_EQ(expectedComputeDispatchAllWalkerEnableNumber, csr->getStreamProperties().frontEndState.computeDispatchAllWalkerEnable.value);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueTest, givenTwoCommandQueuesUsingOneCsrWhenExecuteCommandListsIsCalledThenCorrectSizeOfFrontEndCmdsIsCalculated, IsAtLeastXeCore) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ASSERT_NE(nullptr, csr);

    auto commandQueue1 = new MockCommandQueueHw<FamilyType::gfxCoreFamily>{device, csr, &desc};
    commandQueue1->initialize(false, false, false);

    auto commandQueue2 = new MockCommandQueueHw<FamilyType::gfxCoreFamily>{device, csr, &desc};
    commandQueue2->initialize(false, false, false);

    Mock<::L0::KernelImp> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::KernelImp> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
    cooperativeKernel.setGroupSize(1, 1, 1);

    ze_group_count_t threadGroupDimensions{1, 1, 1};
    auto commandListA = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListA->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);

    CmdListKernelLaunchParams launchParams = {};
    commandListA->appendLaunchKernelWithParams(&defaultKernel, threadGroupDimensions, nullptr, launchParams);
    commandListA->close();

    auto commandListB = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandListB->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);

    launchParams.isCooperative = true;
    commandListB->appendLaunchKernelWithParams(&cooperativeKernel, threadGroupDimensions, nullptr, launchParams);
    commandListB->close();

    ze_command_list_handle_t commandListsA[] = {commandListA->toHandle()};
    ze_command_list_handle_t commandListsB[] = {commandListB->toHandle()};

    auto &productHelper = device->getProductHelper();
    bool dispatchAllWalkerSupport = productHelper.isComputeDispatchAllWalkerEnableInCfeStateRequired(device->getHwInfo());

    auto &currentStreamValue = csr->getStreamProperties().frontEndState.computeDispatchAllWalkerEnable.value;
    EXPECT_EQ(-1, currentStreamValue);

    int32_t expectedCurrentState = dispatchAllWalkerSupport ? 0 : -1;

    commandQueue1->executeCommandLists(1, commandListsA, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(expectedCurrentState, currentStreamValue);

    commandQueue2->executeCommandLists(1, commandListsA, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(expectedCurrentState, currentStreamValue);

    commandQueue2->executeCommandLists(1, commandListsB, nullptr, false, nullptr, nullptr);
    expectedCurrentState = expectedCurrentState != -1 ? 1 : -1;
    EXPECT_EQ(expectedCurrentState, currentStreamValue);

    commandQueue1->executeCommandLists(1, commandListsB, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(expectedCurrentState, currentStreamValue);

    commandQueue2->executeCommandLists(1, commandListsA, nullptr, false, nullptr, nullptr);
    expectedCurrentState = expectedCurrentState != -1 ? 0 : -1;
    EXPECT_EQ(expectedCurrentState, currentStreamValue);

    commandQueue1->executeCommandLists(1, commandListsB, nullptr, false, nullptr, nullptr);
    expectedCurrentState = expectedCurrentState != -1 ? 1 : -1;
    EXPECT_EQ(expectedCurrentState, currentStreamValue);

    commandQueue1->destroy();
    commandQueue2->destroy();
}

TEST(CommandQueue, givenContextGroupEnabledWhenCreatingCommandQueuesThenEachCmdQHasDifferentCsr) {

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_command_queue_desc_t desc = {};
    desc.ordinal = 0;
    desc.index = 0;
    ze_command_queue_handle_t commandQueueHandle1, commandQueueHandle2;

    auto result = device->createCommandQueue(&desc, &commandQueueHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = device->createCommandQueue(&desc, &commandQueueHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandQueue1 = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle1));
    auto commandQueue2 = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle2));

    EXPECT_NE(commandQueue1->getCsr(), commandQueue2->getCsr());

    commandQueue1->destroy();
    commandQueue2->destroy();
}

struct DeferredFirstSubmissionCmdQueueTests : public Test<ModuleFixture> {
    void SetUp() override {
        debugManager.flags.ContextGroupSize.set(5);
        debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.set(1);
        Test<ModuleFixture>::SetUp();
    }

    DebugManagerStateRestore dbgRestorer;
};

HWTEST2_F(DeferredFirstSubmissionCmdQueueTests, givenDebugFlagSetWhenSubmittingToSecondaryThenDeferFirstSubmission, IsAtLeastXeCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    createKernel();
    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_command_queue_desc_t desc = {};
    desc.ordinal = 0;
    desc.index = 0;
    ze_command_queue_handle_t commandQueueHandle1, commandQueueHandle2;

    auto result = device->createCommandQueue(&desc, &commandQueueHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = device->createCommandQueue(&desc, &commandQueueHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandQueue1 = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle1));
    auto commandQueue2 = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle2));

    EXPECT_NE(commandQueue1->getCsr(), commandQueue2->getCsr());

    ze_command_list_handle_t commandListHandle1, commandListHandle2;
    ze_command_list_desc_t cmdListDesc = {};

    result = device->createCommandList(&cmdListDesc, &commandListHandle1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = device->createCommandList(&cmdListDesc, &commandListHandle2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList1 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle1));
    auto commandList2 = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle2));

    auto primaryCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue2->getCsr())->primaryCsr);
    EXPECT_NE(nullptr, primaryCsr);
    EXPECT_NE(commandQueue2->getCsr(), primaryCsr);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    commandList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    commandList2->close();
    EXPECT_EQ(0u, primaryCsr->peekTaskCount());
    EXPECT_EQ(0u, primaryCsr->initializeDeviceWithFirstSubmissionCalled);
    EXPECT_EQ(0u, commandQueue1->getCsr()->peekTaskCount());
    EXPECT_EQ(0u, commandQueue2->getCsr()->peekTaskCount());

    commandQueue2->executeCommandLists(1, &commandListHandle2, nullptr, false, nullptr, nullptr);

    EXPECT_NE(0u, primaryCsr->peekTaskCount());
    EXPECT_EQ(1u, primaryCsr->initializeDeviceWithFirstSubmissionCalled);
    EXPECT_NE(0u, commandQueue2->getCsr()->peekTaskCount());

    commandList1->destroy();
    commandList2->destroy();
    commandQueue1->destroy();
    commandQueue2->destroy();
}

TEST(CommandQueue, givenContextGroupEnabledWhenCreatingCommandQueuesWithInterruptFlagThenPassInterruptRequestToOsContext) {
    class MockOsContext : public OsContext {
      public:
        using OsContext::OsContext;

        bool initializeContext(bool allocateInterrupt) override {
            allocateInterruptPassed = allocateInterrupt;
            return OsContext::initializeContext(allocateInterrupt);
        }

        bool allocateInterruptPassed = false;
    };

    HardwareInfo hwInfo = *defaultHwInfo;
    if (hwInfo.capabilityTable.defaultEngineType != aub_stream::EngineType::ENGINE_CCS) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(5);

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    std::vector<MockOsContext *> mockOsContexts;

    auto neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
    {
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        auto device = driverHandle->devices[0];

        for (auto &engine : neoDevice->secondaryEngines[aub_stream::ENGINE_CCS].engines) {
            EngineDescriptor descriptor({aub_stream::ENGINE_CCS, engine.osContext->getEngineUsage()}, engine.osContext->getDeviceBitfield(), PreemptionMode::Disabled, false);
            auto newOsContext = new MockOsContext(0, 0, descriptor);
            mockOsContexts.push_back(newOsContext);
            newOsContext->incRefInternal();

            newOsContext->setIsPrimaryEngine(engine.osContext->getIsPrimaryEngine());
            newOsContext->setContextGroup(engine.osContext->isPartOfContextGroup());

            engine.osContext = newOsContext;
            engine.commandStreamReceiver->setupContext(*newOsContext);
        }

        zex_intel_queue_allocate_msix_hint_exp_desc_t allocateMsix = {};
        allocateMsix.stype = ZEX_INTEL_STRUCTURE_TYPE_QUEUE_ALLOCATE_MSIX_HINT_EXP_PROPERTIES;
        allocateMsix.uniqueMsix = true;

        ze_command_queue_desc_t desc = {};
        desc.pNext = &allocateMsix;
        ze_command_queue_handle_t commandQueueHandle1, commandQueueHandle2, commandQueueHandle3;

        auto result = device->createCommandQueue(&desc, &commandQueueHandle1);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        result = device->createCommandQueue(&desc, &commandQueueHandle2);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        allocateMsix.uniqueMsix = false;

        result = device->createCommandQueue(&desc, &commandQueueHandle3);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        auto commandQueue1 = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle1));
        auto commandQueue2 = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle2));
        auto commandQueue3 = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle3));

        EXPECT_TRUE(static_cast<MockOsContext &>(commandQueue1->getCsr()->getOsContext()).allocateInterruptPassed);
        EXPECT_TRUE(static_cast<MockOsContext &>(commandQueue2->getCsr()->getOsContext()).allocateInterruptPassed);
        EXPECT_FALSE(static_cast<MockOsContext &>(commandQueue3->getCsr()->getOsContext()).allocateInterruptPassed);

        commandQueue1->destroy();
        commandQueue2->destroy();
        commandQueue3->destroy();
    }

    for (auto &context : mockOsContexts) {
        context->decRefInternal();
    }
}

} // namespace ult
} // namespace L0

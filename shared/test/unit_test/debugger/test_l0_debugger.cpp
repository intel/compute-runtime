/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_source_level_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

TEST(Debugger, givenL0DebuggerWhenGettingL0DebuggerThenCorrectObjectIsReturned) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingEnabled();

    auto hwInfo = *NEO::defaultHwInfo.get();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<NEO::MockDevice> neoDevice(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));

    auto mockDebugger = new MockDebuggerL0(neoDevice.get());
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(mockDebugger);
    auto debugger = neoDevice->getL0Debugger();
    ASSERT_NE(nullptr, debugger);
    EXPECT_EQ(mockDebugger, debugger);
}

TEST(Debugger, givenSourceLevelDebuggerWhenGettingL0DebuggerThenNullptrIsReturned) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingEnabled();

    auto hwInfo = *NEO::defaultHwInfo.get();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<NEO::MockDevice> neoDevice(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));

    auto mockDebugger = new MockSourceLevelDebugger();
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(mockDebugger);
    auto debugger = neoDevice->getL0Debugger();
    EXPECT_EQ(nullptr, debugger);
}

TEST(Debugger, givenL0DebuggerOFFWhenGettingStateSaveAreaHeaderThenValidSipTypeIsReturned) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    auto isHexadecimalArrayPreferred = NEO::HwHelper::get(NEO::defaultHwInfo->platform.eRenderCoreFamily).isSipKernelAsHexadecimalArrayPreferred();
    if (!isHexadecimalArrayPreferred) {
        auto mockBuiltIns = new NEO::MockBuiltins();
        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    }
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<NEO::MockDevice> neoDevice(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));
    auto sipType = SipKernel::getSipKernelType(*neoDevice);

    if (isHexadecimalArrayPreferred) {
        SipKernel::initSipKernel(sipType, *neoDevice);
    }
    auto &stateSaveAreaHeader = SipKernel::getSipKernel(*neoDevice).getStateSaveAreaHeader();

    if (isHexadecimalArrayPreferred) {
        auto sipKernel = neoDevice->getRootDeviceEnvironment().sipKernels[static_cast<uint32_t>(sipType)].get();
        ASSERT_NE(sipKernel, nullptr);
        auto &expectedStateSaveAreaHeader = sipKernel->getStateSaveAreaHeader();
        EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
    } else {
        auto &expectedStateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();
        EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
    }
}

TEST(Debugger, givenDebuggingEnabledInExecEnvWhenAllocatingIsaThenSingleBankIsUsed) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingEnabled();

    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<NEO::MockDevice> neoDevice(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));

    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {neoDevice->getRootDeviceIndex(), 4096, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()});

    if (allocation->getMemoryPool() == MemoryPool::LocalMemory) {
        EXPECT_EQ(1u, allocation->storageInfo.getMemoryBanks());
    } else {
        EXPECT_EQ(0u, allocation->storageInfo.getMemoryBanks());
    }

    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

TEST(Debugger, WhenInitializingDebuggerL0ThenCapabilitiesAreAdjustedAndDebuggerIsCreated) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->incRefInternal();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    auto hwInfo = *NEO::defaultHwInfo.get();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    executionEnvironment->setDebuggingEnabled();

    auto neoDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0u));

    executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(neoDevice.get());

    EXPECT_FALSE(executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.fusedEuEnabled);
    EXPECT_FALSE(executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedImages);

    EXPECT_NE(nullptr, executionEnvironment->rootDeviceEnvironments[0]->debugger);
    executionEnvironment->decRefInternal();
}

TEST(Debugger, GivenLegacyDebuggerWhenInitializingDebuggerL0ThenAbortIsCalledAfterPrintingError) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.PrintDebugMessages.set(1);

    ::testing::internal::CaptureStderr();
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->incRefInternal();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    auto mockDebugger = new MockSourceLevelDebugger();
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(mockDebugger);

    auto hwInfo = *NEO::defaultHwInfo.get();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    executionEnvironment->setDebuggingEnabled();

    auto neoDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0u));

    EXPECT_THROW(executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(neoDevice.get()), std::exception);
    std::string output = testing::internal::GetCapturedStderr();

    EXPECT_EQ(std::string("Source Level Debugger cannot be used with Environment Variable enabling program debugging.\n"), output);

    executionEnvironment->decRefInternal();
}

using L0DebuggerTest = Test<DeviceFixture>;

HWTEST_F(L0DebuggerTest, GivenDeviceWhenAllocateCalledThenDebuggerIsCreated) {
    auto debugger = DebuggerL0Hw<FamilyType>::allocate(pDevice);
    EXPECT_NE(nullptr, debugger);
    delete debugger;
}

HWTEST_F(L0DebuggerTest, givenDebuggerWithoutMemoryOperationsHandlerWhenNotifyingModuleAllocationsThenNoAllocationIsResident) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);

    StackVec<NEO::GraphicsAllocation *, 32> allocs;
    NEO::GraphicsAllocation alloc(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                  MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    allocs.push_back(&alloc);

    debugger->notifyModuleLoadAllocations(allocs);
}

HWTEST_F(L0DebuggerTest, givenDebuggerWhenCreatedThenModuleHeapDebugAreaIsCreated) {
    auto mockBlitMemoryToAllocation = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                         Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        return BlitOperationResult::Success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();

    memoryOperationsHandler->makeResidentCalledCount = 0;
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto neoDevice = pDevice;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    auto debugArea = debugger->getModuleDebugArea();

    EXPECT_EQ(1, memoryOperationsHandler->makeResidentCalledCount);

    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {neoDevice->getRootDeviceIndex(), 4096, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()});

    EXPECT_EQ(allocation->storageInfo.getMemoryBanks(), debugArea->storageInfo.getMemoryBanks());

    DebugAreaHeader *header = reinterpret_cast<DebugAreaHeader *>(debugArea->getUnderlyingBuffer());
    EXPECT_EQ(1u, header->pgsize);
    uint64_t isShared = debugArea->storageInfo.getNumBanks() == 1 ? 1 : 0;
    EXPECT_EQ(isShared, header->isShared);

    EXPECT_STREQ("dbgarea", header->magic);
    EXPECT_EQ(sizeof(DebugAreaHeader), header->size);
    EXPECT_EQ(sizeof(DebugAreaHeader), header->scratchBegin);
    EXPECT_EQ(MemoryConstants::pageSize64k - sizeof(DebugAreaHeader), header->scratchEnd);

    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

HWTEST_F(L0DebuggerTest, givenBindlessSipWhenModuleHeapDebugAreaIsCreatedThenReservedFieldIsSet) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.UseBindlessDebugSip.set(1);

    auto mockBlitMemoryToAllocation = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                         Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        return BlitOperationResult::Success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    memoryOperationsHandler->makeResidentCalledCount = 0;
    auto neoDevice = pDevice;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    auto debugArea = debugger->getModuleDebugArea();

    DebugAreaHeader *header = reinterpret_cast<DebugAreaHeader *>(debugArea->getUnderlyingBuffer());
    EXPECT_EQ(1u, header->reserved1);
}

HWTEST_F(L0DebuggerTest, givenUseBindlessDebugSipZeroWhenModuleHeapDebugAreaIsCreatedThenReservedFieldIsSet) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.UseBindlessDebugSip.set(0);

    auto mockBlitMemoryToAllocation = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                         Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        return BlitOperationResult::Success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    memoryOperationsHandler->makeResidentCalledCount = 0;
    auto neoDevice = pDevice;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    auto debugArea = debugger->getModuleDebugArea();

    DebugAreaHeader *header = reinterpret_cast<DebugAreaHeader *>(debugArea->getUnderlyingBuffer());
    EXPECT_EQ(1u, header->reserved1);
}

TEST(Debugger, givenNonLegacyDebuggerWhenInitializingDeviceCapsThenUnrecoverableIsCalled) {
    class MockDebugger : public NEO::Debugger {
      public:
        MockDebugger() {
            isLegacyMode = false;
        }

        void captureStateBaseAddress(NEO::LinearStream &cmdStream, SbaAddresses sba) override{};
        size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) override {
            return 0;
        }
    };
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    auto mockBuiltIns = new NEO::MockBuiltins();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto debugger = new MockDebugger;
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(debugger);
    executionEnvironment->initializeMemoryManager();

    EXPECT_THROW(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u), std::exception);
}

using PerContextAddressSpaceL0DebuggerTest = L0DebuggerTest;

HWTEST_F(PerContextAddressSpaceL0DebuggerTest, givenCanonizedGpuVasWhenProgrammingSbaTrackingThenNonCanonicalAddressesAreStored) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(0);

    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    auto expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, GeneralStateBaseAddress);

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0xffff800000060000;
    uint64_t ssba = 0xffff801234567000;
    uint64_t iba = 0xffff8000fff80000;
    uint64_t ioba = 0xffff800008100000;
    uint64_t dsba = 0xffff8000aaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;
    sbaAddresses.InstructionBaseAddress = iba;
    sbaAddresses.IndirectObjectBaseAddress = ioba;
    sbaAddresses.DynamicStateBaseAddress = dsba;
    sbaAddresses.BindlessSurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    EXPECT_EQ(6 * sizeof(MI_STORE_DATA_IMM), cmdStream.getUsed());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(static_cast<uint32_t>(gsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(gsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, SurfaceStateBaseAddress);

    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, DynamicStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(dsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(dsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, IndirectObjectBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ioba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ioba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, InstructionBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(iba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(iba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, BindlessSurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());
}

HWTEST_F(PerContextAddressSpaceL0DebuggerTest, givenNonZeroGpuVasWhenProgrammingSbaTrackingThenCorrectCmdsAreAddedToStream) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(0);

    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->singleAddressSpaceSbaTracking = 0;
    debugger->sbaTrackingGpuVa.address = 0x45670000;
    auto expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, GeneralStateBaseAddress);

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0x60000;
    uint64_t ssba = 0x1234567000;
    uint64_t iba = 0xfff80000;
    uint64_t ioba = 0x8100000;
    uint64_t dsba = 0xffffffffaaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;
    sbaAddresses.InstructionBaseAddress = iba;
    sbaAddresses.IndirectObjectBaseAddress = ioba;
    sbaAddresses.DynamicStateBaseAddress = dsba;
    sbaAddresses.BindlessSurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    EXPECT_EQ(6 * sizeof(MI_STORE_DATA_IMM), cmdStream.getUsed());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(static_cast<uint32_t>(gsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(gsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, SurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, DynamicStateBaseAddress);

    EXPECT_EQ(static_cast<uint32_t>(dsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(dsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, IndirectObjectBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ioba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ioba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, InstructionBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(iba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(iba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, BindlessSurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());
}

using L0DebuggerMultiSubDeviceTest = L0DebuggerTest;

HWTEST_F(L0DebuggerMultiSubDeviceTest, givenMultiSubDevicesWhenSbaTrackingBuffersAllocatedThenThereIsSeparatePhysicalStorageForEveryContext) {

    if (is32bit) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(1);
    constexpr auto numSubDevices = 2u;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    auto executionEnvironment = new NEO::ExecutionEnvironment;
    auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    auto neoDevice = devices[0].get();

    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    const auto &engines = neoDevice->getAllEngines();
    EXPECT_LE(1u, engines.size());

    for (auto &engine : engines) {

        auto contextId = engine.osContext->getContextId();
        const auto &storageInfo = debugger->perContextSbaAllocations[contextId]->storageInfo;

        EXPECT_FALSE(storageInfo.cloningOfPageTables);
        EXPECT_EQ(DeviceBitfield{maxNBitValue(numSubDevices)}, storageInfo.memoryBanks);
        EXPECT_EQ(DeviceBitfield{maxNBitValue(numSubDevices)}, storageInfo.pageTablesVisibility);
        EXPECT_EQ(engine.osContext->getDeviceBitfield().to_ulong(), storageInfo.memoryBanks.to_ulong());
        EXPECT_TRUE(storageInfo.tileInstanced);

        for (uint32_t i = 0; i < numSubDevices; i++) {
            auto sbaHeader = reinterpret_cast<NEO::SbaTrackedAddresses *>(ptrOffset(debugger->perContextSbaAllocations[contextId]->getUnderlyingBuffer(),
                                                                                    debugger->perContextSbaAllocations[contextId]->getUnderlyingBufferSize() * i));

            EXPECT_STREQ("sbaarea", sbaHeader->magic);
            EXPECT_EQ(0u, sbaHeader->BindlessSamplerStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->BindlessSurfaceStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->DynamicStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->GeneralStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->IndirectObjectBaseAddress);
            EXPECT_EQ(0u, sbaHeader->InstructionBaseAddress);
            EXPECT_EQ(0u, sbaHeader->SurfaceStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->Version);
        }
        if (!debugger->singleAddressSpaceSbaTracking) {
            EXPECT_EQ(debugger->sbaTrackingGpuVa.address, debugger->perContextSbaAllocations[contextId]->getGpuAddress());
        } else {
            EXPECT_NE(debugger->sbaTrackingGpuVa.address, debugger->perContextSbaAllocations[contextId]->getGpuAddress());
        }
    }

    const auto &subDevice0Engines = neoDevice->getSubDevice(0)->getAllEngines();
    const auto &subDevice1Engines = neoDevice->getSubDevice(1)->getAllEngines();

    auto subDeviceEngineSets = {subDevice0Engines, subDevice1Engines};
    uint64_t subDeviceIndex = 0;
    for (const auto &subDeviceEngines : subDeviceEngineSets) {
        for (auto &engine : subDeviceEngines) {

            auto contextId = engine.osContext->getContextId();
            const auto &storageInfo = debugger->perContextSbaAllocations[contextId]->storageInfo;

            EXPECT_FALSE(storageInfo.cloningOfPageTables);
            EXPECT_EQ(DeviceBitfield{1llu << subDeviceIndex}, storageInfo.memoryBanks);
            EXPECT_EQ(DeviceBitfield{1llu << subDeviceIndex}, storageInfo.pageTablesVisibility);
            EXPECT_EQ(engine.osContext->getDeviceBitfield().to_ulong(), storageInfo.memoryBanks.to_ulong());
            EXPECT_FALSE(storageInfo.tileInstanced);

            if (!debugger->singleAddressSpaceSbaTracking) {
                EXPECT_EQ(debugger->sbaTrackingGpuVa.address, debugger->perContextSbaAllocations[contextId]->getGpuAddress());
            } else {
                EXPECT_NE(debugger->sbaTrackingGpuVa.address, debugger->perContextSbaAllocations[contextId]->getGpuAddress());
            }
        }
        subDeviceIndex++;
    }
}

struct L0DebuggerSimpleParameterizedTest : public ::testing::TestWithParam<int>, DeviceFixture {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(GetParam());
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    DebugManagerStateRestore restorer;
};

using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;

HWTEST2_P(L0DebuggerSimpleParameterizedTest, givenZeroGpuVasWhenProgrammingSbaTrackingThenStreamIsNotUsed, Gen12Plus) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0;
    uint64_t ssba = 0;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    EXPECT_EQ(0u, cmdStream.getUsed());
}

HWTEST2_P(L0DebuggerSimpleParameterizedTest, givenNotChangedSurfaceStateWhenCapturingSBAThenNoTrackingCmdsAreAdded, Gen12Plus) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;

    NEO::CommandContainer container;
    container.initialize(pDevice, nullptr, true);

    NEO::Debugger::SbaAddresses sba = {};
    sba.SurfaceStateBaseAddress = 0x123456000;

    debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
    auto sizeUsed = container.getCommandStream()->getUsed();

    EXPECT_NE(0u, sizeUsed);
    sba.SurfaceStateBaseAddress = 0;

    debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
    auto sizeUsed2 = container.getCommandStream()->getUsed();

    EXPECT_EQ(sizeUsed, sizeUsed2);
}

HWTEST2_P(L0DebuggerSimpleParameterizedTest, givenChangedBaseAddressesWhenCapturingSBAThenTrackingCmdsAreAdded, Gen12Plus) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    {
        NEO::CommandContainer container;
        container.initialize(pDevice, nullptr, true);

        NEO::Debugger::SbaAddresses sba = {};
        sba.SurfaceStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(pDevice, nullptr, true);

        NEO::Debugger::SbaAddresses sba = {};
        sba.GeneralStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(pDevice, nullptr, true);

        NEO::Debugger::SbaAddresses sba = {};
        sba.BindlessSurfaceStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }
}

INSTANTIATE_TEST_CASE_P(SBAModesForDebugger, L0DebuggerSimpleParameterizedTest, ::testing::Values(0, 1));

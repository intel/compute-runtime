/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/blit_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

TEST(Debugger, givenL0DebuggerWhenGettingL0DebuggerThenCorrectObjectIsReturned) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);

    auto hwInfo = *NEO::defaultHwInfo.get();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<NEO::MockDevice> neoDevice(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));

    auto mockDebugger = new MockDebuggerL0(neoDevice.get());
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(mockDebugger);
    auto debugger = neoDevice->getL0Debugger();
    ASSERT_NE(nullptr, debugger);
    EXPECT_EQ(mockDebugger, debugger);
}

TEST(Debugger, givenL0DebuggerOFFWhenGettingStateSaveAreaHeaderThenValidSipTypeIsReturned) {
    VariableBackup<bool> mockSipBackup(&MockSipData::useMockSip, false);
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto isHexadecimalArrayPreferred = gfxCoreHelper.isSipKernelAsHexadecimalArrayPreferred();
    if (!isHexadecimalArrayPreferred) {
        auto mockBuiltIns = new NEO::MockBuiltins();
        MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    }

    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<NEO::MockDevice> neoDevice(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));
    auto sipType = SipKernel::getSipKernelType(*neoDevice);

    if (isHexadecimalArrayPreferred) {
        SipKernel::initSipKernel(sipType, *neoDevice);
    }
    auto &stateSaveAreaHeader = SipKernel::getSipKernel(*neoDevice, nullptr).getStateSaveAreaHeader();

    if (isHexadecimalArrayPreferred) {
        auto sipKernel = neoDevice->getRootDeviceEnvironment().sipKernels[static_cast<uint32_t>(sipType)].get();
        ASSERT_NE(sipKernel, nullptr);
        auto &expectedStateSaveAreaHeader = sipKernel->getStateSaveAreaHeader();
        EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
    } else {
        auto &expectedStateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();
        EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
    }

    SipKernel::freeSipKernels(&neoDevice->getRootDeviceEnvironmentRef(), neoDevice->getMemoryManager());
}

TEST(Debugger, givenDebuggingEnabledInExecEnvWhenAllocatingIsaThenSingleBankIsUsed) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);

    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<NEO::MockDevice> neoDevice(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));

    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {neoDevice->getRootDeviceIndex(), 4096, NEO::AllocationType::kernelIsa, neoDevice->getDeviceBitfield()});

    if (allocation->getMemoryPool() == MemoryPool::localMemory) {
        EXPECT_EQ(1u, allocation->storageInfo.getMemoryBanks());
    } else {
        EXPECT_EQ(0u, allocation->storageInfo.getMemoryBanks());
    }

    neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

TEST(Debugger, givenTileAttachAndDebuggingEnabledInExecEnvWhenAllocatingIsaThenMultipleBanksAreUsed) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ExperimentalEnableTileAttach.set(1);

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);

    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    std::unique_ptr<NEO::MockDevice> neoDevice(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));

    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {neoDevice->getRootDeviceIndex(), 4096, NEO::AllocationType::kernelIsa, DeviceBitfield{3}});

    if (allocation->getMemoryPool() == MemoryPool::localMemory) {
        EXPECT_EQ(3u, allocation->storageInfo.getMemoryBanks());
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
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);

    auto neoDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0u));

    executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(neoDevice.get());

    EXPECT_FALSE(executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.fusedEuEnabled);
    EXPECT_FALSE(executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedImages);

    EXPECT_NE(nullptr, executionEnvironment->rootDeviceEnvironments[0]->debugger);
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
    debugger->initialize();

    StackVec<NEO::GraphicsAllocation *, 32> allocs;
    NEO::GraphicsAllocation alloc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    allocs.push_back(&alloc);

    debugger->notifyModuleLoadAllocations(pDevice, allocs);
}

HWTEST_F(L0DebuggerTest, givenDebuggerWhenCreatedThenModuleHeapDebugAreaIsCreated) {
    auto mockBlitMemoryToAllocation = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                         Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        return BlitOperationResult::success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();

    memoryOperationsHandler->makeResidentCalledCount = 0;
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto neoDevice = pDevice;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    debugger->initialize();
    auto debugArea = debugger->getModuleDebugArea();

    EXPECT_EQ(1, memoryOperationsHandler->makeResidentCalledCount);

    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {neoDevice->getRootDeviceIndex(), 4096, NEO::AllocationType::kernelIsa, neoDevice->getDeviceBitfield()});

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

HWTEST_F(L0DebuggerTest, givenDebuggerCreatedWhenSubdevicesExistThenModuleHeapDebugAreaIsResidentForSubDevices) {
    DebugManagerStateRestore restorer;
    constexpr auto numSubDevices = 2;
    debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
    auto executionEnvironment = new NEO::ExecutionEnvironment;
    auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    auto neoDevice = devices[0].get();

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    memoryOperationsHandler->makeResidentCalledCount = 0;
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    debugger->initialize();

    EXPECT_EQ((1 + numSubDevices), memoryOperationsHandler->makeResidentCalledCount);
}

HWTEST_F(L0DebuggerTest, givenBindlessSipWhenModuleHeapDebugAreaIsCreatedThenReservedFieldIsSet) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseBindlessDebugSip.set(1);

    auto mockBlitMemoryToAllocation = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                         Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        return BlitOperationResult::success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    memoryOperationsHandler->makeResidentCalledCount = 0;
    auto neoDevice = pDevice;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    debugger->initialize();
    auto debugArea = debugger->getModuleDebugArea();

    DebugAreaHeader *header = reinterpret_cast<DebugAreaHeader *>(debugArea->getUnderlyingBuffer());
    EXPECT_EQ(1u, header->reserved1);
}

HWTEST_F(L0DebuggerTest, givenUseBindlessDebugSipZeroWhenModuleHeapDebugAreaIsCreatedThenReservedFieldIsSet) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.UseBindlessDebugSip.set(0);

    auto mockBlitMemoryToAllocation = [](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                         Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        return BlitOperationResult::success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    memoryOperationsHandler->makeResidentCalledCount = 0;
    auto neoDevice = pDevice;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    debugger->initialize();
    auto debugArea = debugger->getModuleDebugArea();

    DebugAreaHeader *header = reinterpret_cast<DebugAreaHeader *>(debugArea->getUnderlyingBuffer());
    EXPECT_EQ(1u, header->reserved1);
}

HWTEST_F(L0DebuggerTest, givenProgramDebuggingWhenGettingDebuggingModeThenCorrectModeIsReturned) {
    DebuggingMode mode = NEO::getDebuggingMode(0);
    EXPECT_TRUE(DebuggingMode::disabled == mode);

    mode = NEO::getDebuggingMode(1);
    EXPECT_TRUE(DebuggingMode::online == mode);

    mode = NEO::getDebuggingMode(2);
    EXPECT_TRUE(DebuggingMode::offline == mode);

    mode = NEO::getDebuggingMode(3);
    EXPECT_TRUE(DebuggingMode::disabled == mode);
}

using PerContextAddressSpaceL0DebuggerTest = L0DebuggerTest;

HWTEST_F(PerContextAddressSpaceL0DebuggerTest, givenCanonizedGpuVasWhenProgrammingSbaTrackingThenNonCanonicalAddressesAreStored) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(0);

    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    auto expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, generalStateBaseAddress);

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0xffff800000060000;
    uint64_t ssba = 0xffff801234567000;
    uint64_t iba = 0xffff8000fff80000;
    uint64_t ioba = 0xffff800008100000;
    uint64_t dsba = 0xffff8000aaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.generalStateBaseAddress = gsba;
    sbaAddresses.surfaceStateBaseAddress = ssba;
    sbaAddresses.instructionBaseAddress = iba;
    sbaAddresses.indirectObjectBaseAddress = ioba;
    sbaAddresses.dynamicStateBaseAddress = dsba;
    sbaAddresses.bindlessSurfaceStateBaseAddress = ssba;

    debugger->captureStateBaseAddress(cmdStream, sbaAddresses, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

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

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, surfaceStateBaseAddress);

    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, dynamicStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(dsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(dsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, indirectObjectBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ioba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ioba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, instructionBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(iba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(iba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, bindlessSurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());
}

HWTEST_F(PerContextAddressSpaceL0DebuggerTest, givenNonZeroGpuVasWhenProgrammingSbaTrackingThenCorrectCmdsAreAddedToStream) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(0);

    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();
    debugger->singleAddressSpaceSbaTracking = 0;
    debugger->sbaTrackingGpuVa.address = 0x45670000;
    auto expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, generalStateBaseAddress);

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0x60000;
    uint64_t ssba = 0x1234567000;
    uint64_t iba = 0xfff80000;
    uint64_t ioba = 0x8100000;
    uint64_t dsba = 0xffffffffaaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.generalStateBaseAddress = gsba;
    sbaAddresses.surfaceStateBaseAddress = ssba;
    sbaAddresses.instructionBaseAddress = iba;
    sbaAddresses.indirectObjectBaseAddress = ioba;
    sbaAddresses.dynamicStateBaseAddress = dsba;
    sbaAddresses.bindlessSurfaceStateBaseAddress = ssba;

    debugger->captureStateBaseAddress(cmdStream, sbaAddresses, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

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

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, surfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, dynamicStateBaseAddress);

    EXPECT_EQ(static_cast<uint32_t>(dsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(dsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, indirectObjectBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ioba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ioba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, instructionBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(iba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(iba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, bindlessSurfaceStateBaseAddress);
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
    debugManager.flags.CreateMultipleRootDevices.set(1);
    constexpr auto numSubDevices = 2u;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    auto executionEnvironment = new NEO::ExecutionEnvironment;
    auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    auto neoDevice = devices[0].get();

    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    debugger->initialize();

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
            EXPECT_EQ(0u, sbaHeader->bindlessSamplerStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->bindlessSurfaceStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->dynamicStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->generalStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->indirectObjectBaseAddress);
            EXPECT_EQ(0u, sbaHeader->instructionBaseAddress);
            EXPECT_EQ(0u, sbaHeader->surfaceStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->version);
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
        NEO::debugManager.flags.DebuggerForceSbaTrackingMode.set(GetParam());
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }

    DebugManagerStateRestore restorer;
};

using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;

HWTEST2_P(L0DebuggerSimpleParameterizedTest, givenZeroGpuVasWhenProgrammingSbaTrackingThenStreamIsNotUsed, Gen12Plus) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();

    debugger->sbaTrackingGpuVa.address = 0x45670000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0;
    uint64_t ssba = 0;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.generalStateBaseAddress = gsba;
    sbaAddresses.surfaceStateBaseAddress = ssba;

    debugger->captureStateBaseAddress(cmdStream, sbaAddresses, false);

    EXPECT_EQ(0u, cmdStream.getUsed());
}

HWTEST2_P(L0DebuggerSimpleParameterizedTest, givenNotChangedSurfaceStateWhenCapturingSBAThenNoTrackingCmdsAreAdded, Gen12Plus) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();

    debugger->sbaTrackingGpuVa.address = 0x45670000;

    NEO::CommandContainer container;
    container.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

    NEO::Debugger::SbaAddresses sba = {};
    sba.surfaceStateBaseAddress = 0x123456000;

    debugger->captureStateBaseAddress(*container.getCommandStream(), sba, false);
    auto sizeUsed = container.getCommandStream()->getUsed();

    EXPECT_NE(0u, sizeUsed);
    sba.surfaceStateBaseAddress = 0;

    debugger->captureStateBaseAddress(*container.getCommandStream(), sba, false);
    auto sizeUsed2 = container.getCommandStream()->getUsed();

    EXPECT_EQ(sizeUsed, sizeUsed2);
}

HWTEST2_P(L0DebuggerSimpleParameterizedTest, givenChangedBaseAddressesWhenCapturingSBAThenTrackingCmdsAreAdded, Gen12Plus) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    {
        NEO::CommandContainer container;
        container.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

        NEO::Debugger::SbaAddresses sba = {};
        sba.surfaceStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(*container.getCommandStream(), sba, false);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

        NEO::Debugger::SbaAddresses sba = {};
        sba.generalStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(*container.getCommandStream(), sba, false);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);

        NEO::Debugger::SbaAddresses sba = {};
        sba.bindlessSurfaceStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(*container.getCommandStream(), sba, false);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }
}

INSTANTIATE_TEST_SUITE_P(SBAModesForDebugger, L0DebuggerSimpleParameterizedTest, ::testing::Values(0, 1));

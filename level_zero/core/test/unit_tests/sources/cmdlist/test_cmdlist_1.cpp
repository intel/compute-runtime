/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using ContextCommandListCreate = Test<ContextFixture>;

TEST_F(ContextCommandListCreate, whenCreatingCommandListFromContextThenSuccessIsReturned) {
    ze_command_list_desc_t desc = {};
    ze_command_list_handle_t hCommandList = {};

    ze_result_t result = context->createCommandList(device, &desc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::CommandList *commandList = L0::CommandList::fromHandle(hCommandList);
    commandList->destroy();
}

TEST_F(ContextCommandListCreate, whenCreatingCommandListImmediateFromContextThenSuccessIsReturned) {
    ze_command_queue_desc_t desc = {};
    ze_command_list_handle_t hCommandList = {};

    ze_result_t result = context->createCommandListImmediate(device, &desc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    L0::CommandList *commandList = L0::CommandList::fromHandle(hCommandList);
    commandList->destroy();
}

using CommandListCreate = Test<DeviceFixture>;

TEST_F(CommandListCreate, whenCommandListIsCreatedWithInvalidProductFamilyThenFailureIsReturned) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(PRODUCT_FAMILY::IGFX_MAX_PRODUCT, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, returnValue);
    ASSERT_EQ(nullptr, commandList);
}

TEST_F(CommandListCreate, whenCommandListImmediateIsCreatedWithInvalidProductFamilyThenFailureIsReturned) {
    ze_result_t returnValue;
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(PRODUCT_FAMILY::IGFX_MAX_PRODUCT,
                                                                              device,
                                                                              &desc,
                                                                              internalEngine,
                                                                              NEO::EngineGroupType::RenderCompute,
                                                                              returnValue));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, returnValue);
    ASSERT_EQ(nullptr, commandList);
}

TEST_F(CommandListCreate, whenCommandListIsCreatedThenItIsInitialized) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    ASSERT_GT(commandList->commandContainer.getCmdBufferAllocations().size(), 0u);

    auto numAllocations = 0u;
    auto allocation = whitebox_cast(commandList->commandContainer.getCmdBufferAllocations()[0]);
    ASSERT_NE(allocation, nullptr);

    ++numAllocations;

    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    for (uint32_t i = 0; i < NEO::HeapType::NUM_TYPES; i++) {
        ASSERT_NE(commandList->commandContainer.getIndirectHeap(static_cast<NEO::HeapType>(i)), nullptr);
        ++numAllocations;
        ASSERT_NE(commandList->commandContainer.getIndirectHeapAllocation(static_cast<NEO::HeapType>(i)), nullptr);
    }

    EXPECT_LT(0u, commandList->commandContainer.getCommandStream()->getAvailableSpace());
    ASSERT_EQ(commandList->commandContainer.getResidencyContainer().size(), numAllocations);
    EXPECT_EQ(commandList->commandContainer.getResidencyContainer().front(), allocation);
}

TEST_F(CommandListCreate, givenRegularCommandListThenDefaultNumIddPerBlockIsUsed) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    const uint32_t defaultNumIdds = CommandList::defaultNumIddsPerBlock;
    EXPECT_EQ(defaultNumIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemAdviseReturnsError) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto res = commandList->appendMemAdvise(device, nullptr, 0, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemoryPrefetchReturnsError) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto res = commandList->appendMemoryPrefetch(nullptr, 0);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

TEST_F(CommandListCreate, givenValidPtrWhenAppendMemAdviseFailsThenReturnError) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->failSetMemAdvise = true;

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrWhenAppendMemAdviseSucceedsThenReturnSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseSetWithMaxHintThenSuccessReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_FORCE_UINT32);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseSetAndClearReadMostlyThenMemAdviseReadOnlySet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.read_only);
    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.read_only);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseSetAndClearPreferredLocationThenMemAdvisePreferredDeviceSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.device_preferred_location);
    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.device_preferred_location);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseSetAndClearNonAtomicThenMemAdviseNonAtomicSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_NON_ATOMIC_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.non_atomic);
    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_NON_ATOMIC_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.non_atomic);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseSetAndClearCachingThenMemAdviseCachingSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_BIAS_CACHED);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.cached_memory);
    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_EQ(1, memoryManager->memAdviseFlags.cached_memory);
    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_BIAS_UNCACHED);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cached_memory);
    EXPECT_EQ(0, memoryManager->memAdviseFlags.cached_memory);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

using CommandListMemAdvisePageFault = Test<PageFaultDeviceFixture>;

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerThenAppendMemAdviseWithReadOnlyAndDevicePreferredClearsMigrationBlocked) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    flags.cpu_migration_blocked = 1;
    deviceImp->memAdviseSharedAllocations[allocData] = flags;

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.read_only);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.device_preferred_location);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.read_only);
    EXPECT_EQ(0, flags.device_preferred_location);
    EXPECT_EQ(0, flags.cpu_migration_blocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerThenGpuDomainHanlderWithHintsIsSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    deviceImp->memAdviseSharedAllocations[allocData] = flags;

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.read_only);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.device_preferred_location);

    auto handlerWithHints = L0::handleGpuDomainTransferForHwWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetThenHandlerBlocksCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.read_only);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.device_preferred_location);

    auto handlerWithHints = L0::handleGpuDomainTransferForHwWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::Gpu;
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.cpu_migration_blocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetAndOnlyReadOnlyOrDevicePreferredHintThenHandlerAllowsCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.read_only);

    auto handlerWithHints = L0::handleGpuDomainTransferForHwWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::Gpu;
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpu_migration_blocked);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.read_only);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.device_preferred_location);

    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpu_migration_blocked);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.device_preferred_location);

    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpu_migration_blocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetAndWithPrintUsmSharedMigrationDebugKeyThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintUmdSharedMigration.set(1);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto res = context->allocSharedMem(device->toHandle(),
                                       &deviceDesc,
                                       &hostDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.read_only);

    auto handlerWithHints = L0::handleGpuDomainTransferForHwWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    testing::internal::CaptureStdout(); // start capturing

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::Gpu;
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpu_migration_blocked);

    std::string output = testing::internal::GetCapturedStdout(); // stop capturing

    std::string expectedString = "UMD transferring shared allocation";
    uint32_t occurrences = 0u;
    uint32_t expectedOccurrences = 1u;
    size_t idx = output.find(expectedString);
    while (idx != std::string::npos) {
        occurrences++;
        idx = output.find(expectedString, idx + 1);
    }
    EXPECT_EQ(expectedOccurrences, occurrences);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetAndInvalidHintsThenHandlerAllowsCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_BIAS_CACHED);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.cached_memory);

    auto handlerWithHints = L0::handleGpuDomainTransferForHwWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::Gpu;
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpu_migration_blocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetAndCpuDomainThenHandlerAllowsCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.read_only);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.device_preferred_location);

    auto handlerWithHints = L0::handleGpuDomainTransferForHwWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::Cpu;
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpu_migration_blocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenInvalidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetThenHandlerAllowsCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.read_only);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.device_preferred_location);

    auto handlerWithHints = L0::handleGpuDomainTransferForHwWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::Gpu;
    void *alloc = reinterpret_cast<void *>(0x1);
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, alloc, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpu_migration_blocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemoryPrefetchReturnsSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenImmediateCommandListThenInternalEngineIsUsedIfRequested) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    internalEngine = false;

    std::unique_ptr<L0::CommandList> commandList1(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList1);

    cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList1->cmdQImmediate);
    EXPECT_NE(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);
}

TEST_F(CommandListCreate, givenInternalUsageCommandListThenIsInternalReturnsTrue) {
    const ze_command_queue_desc_t desc = {};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               true,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               returnValue));

    EXPECT_TRUE(commandList0->isInternal());
}

TEST_F(CommandListCreate, givenNonInternalUsageCommandListThenIsInternalReturnsFalse) {
    const ze_command_queue_desc_t desc = {};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               returnValue));

    EXPECT_FALSE(commandList0->isInternal());
}

TEST_F(CommandListCreate, givenImmediateCommandListThenCustomNumIddPerBlockUsed) {
    const ze_command_queue_desc_t desc = {};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    const uint32_t cmdListImmediateIdds = CommandList::commandListimmediateIddsPerBlock;
    EXPECT_EQ(cmdListImmediateIdds, commandList->commandContainer.getNumIddPerBlock());
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListThenItHasImmediateCommandQueueCreated) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListWithSyncModeThenItHasImmediateCommandQueueCreated) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListWithASyncModeThenItHasImmediateCommandQueueCreated) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithSyncModeAndAppendSignalEventThenUpdateTaskCountNeededFlagIsDisabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendSignalEvent(event);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithSyncModeAndAppendBarrierThenUpdateTaskCountNeededFlagIsDisabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendBarrier(nullptr, 1, &event);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);

    commandList->appendBarrier(nullptr, 0, nullptr);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithSyncModeAndAppendResetEventThenUpdateTaskCountNeededFlagIsDisabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendEventReset(event);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndAppendSignalEventThenUpdateTaskCountNeededFlagIsEnabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendSignalEvent(event);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndAppendBarrierThenUpdateTaskCountNeededFlagIsEnabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendBarrier(event, 0, nullptr);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);

    commandList->appendBarrier(nullptr, 0, nullptr);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndCopyEngineAndAppendBarrierThenUpdateTaskCountNeededFlagIsEnabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendBarrier(event, 0, nullptr);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);

    commandList->appendBarrier(nullptr, 0, nullptr);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndAppendEventResetThenUpdateTaskCountNeededFlagIsEnabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> event_object(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, event_object->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver, event_object->csr);

    commandList->appendEventReset(event);

    auto result = event_object->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(event_object->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, whenInvokingAppendMemoryCopyFromContextForImmediateCommandListWithSyncModeThenSuccessIsReturned) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

struct CommandListCreateWithDeferredOsContextInitialization : ContextCommandListCreate {
    void SetUp() override {
        DebugManager.flags.DeferOsContextInitialization.set(1);
        ContextCommandListCreate::SetUp();
    }

    void TearDown() override {
        ContextCommandListCreate::TearDown();
    }

    DebugManagerStateRestore restore;
};
TEST_F(ContextCommandListCreate, givenDeferredEngineCreationWhenImmediateCommandListIsCreatedThenEngineIsInitialized) {
    uint32_t groupsCount{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getCommandQueueGroupProperties(&groupsCount, nullptr));
    auto groups = std::vector<ze_command_queue_group_properties_t>(groupsCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getCommandQueueGroupProperties(&groupsCount, groups.data()));

    for (uint32_t groupIndex = 0u; groupIndex < groupsCount; groupIndex++) {
        const auto &group = groups[groupIndex];
        for (uint32_t queueIndex = 0; queueIndex < group.numQueues; queueIndex++) {
            CommandStreamReceiver *expectedCsr{};
            EXPECT_EQ(ZE_RESULT_SUCCESS, device->getCsrForOrdinalAndIndex(&expectedCsr, groupIndex, queueIndex));

            ze_command_queue_desc_t desc = {};
            desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
            desc.ordinal = groupIndex;
            desc.index = queueIndex;
            ze_command_list_handle_t cmdListHandle;
            ze_result_t result = context->createCommandListImmediate(device, &desc, &cmdListHandle);
            L0::CommandList *cmdList = L0::CommandList::fromHandle(cmdListHandle);

            EXPECT_EQ(device, cmdList->device);
            EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, cmdList->cmdListType);
            EXPECT_NE(nullptr, cmdList);
            EXPECT_EQ(ZE_RESULT_SUCCESS, result);
            EXPECT_TRUE(expectedCsr->getOsContext().isInitialized());
            EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->destroy());
        }
    }
}

TEST_F(CommandListCreate, whenInvokingAppendMemoryCopyFromContextForImmediateCommandListWithASyncModeThenSuccessIsReturned) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(CommandListCreate, whenInvokingAppendMemoryCopyFromContextForImmediateCommandListThenSuccessIsReturned) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(CommandListCreate, givenQueueDescriptionwhenCreatingImmediateCommandListForEveryEnigneThenItHasImmediateCommandQueueCreated) {
    auto &engines = neoDevice->getEngineGroups();
    uint32_t numaAvailableEngineGroups = 0;
    for (uint32_t ordinal = 0; ordinal < CommonConstants::engineGroupCount; ordinal++) {
        if (engines[ordinal].size()) {
            numaAvailableEngineGroups++;
        }
    }
    for (uint32_t ordinal = 0; ordinal < numaAvailableEngineGroups; ordinal++) {
        uint32_t engineGroupIndex = ordinal;
        device->mapOrdinalForAvailableEngineGroup(&engineGroupIndex);
        for (uint32_t index = 0; index < engines[engineGroupIndex].size(); index++) {
            ze_command_queue_desc_t desc = {};
            desc.ordinal = ordinal;
            desc.index = index;
            ze_result_t returnValue;
            std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
            ASSERT_NE(nullptr, commandList);

            EXPECT_EQ(device, commandList->device);
            EXPECT_EQ(CommandList::CommandListType::TYPE_IMMEDIATE, commandList->cmdListType);
            EXPECT_NE(nullptr, commandList->cmdQImmediate);
        }
    }
}

TEST_F(CommandListCreate, givenInvalidProductFamilyThenReturnsNullPointer) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(IGFX_UNKNOWN, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(nullptr, commandList);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListCreate, whenCommandListIsCreatedThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(0);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    auto gmmHelper = commandContainer.getDevice()->getGmmHelper();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPc = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPc);
    auto cmdPc = genCmdCast<PIPE_CONTROL *>(*itorPc);
    EXPECT_TRUE(cmdPc->getDcFlushEnable());
    EXPECT_TRUE(cmdPc->getCommandStreamerStallEnable());
    EXPECT_TRUE(cmdPc->getTextureCacheInvalidationEnable());

    auto itor = find<STATE_BASE_ADDRESS *>(itorPc, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);

    auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
    auto ioh = commandContainer.getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);
    auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);

    EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
    EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());

    EXPECT_TRUE(cmdSba->getIndirectObjectBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(ioh->getHeapGpuBase(), cmdSba->getIndirectObjectBaseAddress());
    EXPECT_EQ(ioh->getHeapSizeInPages(), cmdSba->getIndirectObjectBufferSize());

    EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListCreate, whenBindlessModeEnabledWhenCommandListIsCreatedThenStateBaseAddressCmdsIsNotAdded) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itor);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListCreate, whenBindlessModeEnabledWhenCommandListImmediateIsCreatedThenStateBaseAddressCmdsIsNotAdded) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    auto &commandContainer = commandList->commandContainer;

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenCreatedThenStateBaseAddressCmdIsNotProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenSetBarrierThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    commandList->appendBarrier(nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenImmediateCommandListWithCopyOnlyWhenSetBarrierThenMiFlushCmdIsNotInsertedInTheCmdContainer) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    auto &commandContainer = commandList->commandContainer;
    commandList->appendBarrier(nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, whenCommandListIsResetThenContainsStatelessUncachedResourceIsSetToFalse) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::Compute,
                                                                     0u,
                                                                     returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_FALSE(commandList->getContainsStatelessUncachedResource());
}

HWTEST_F(CommandListCreate, givenBindlessModeEnabledWhenCommandListsResetThenSbaNotReloaded) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::Compute,
                                                                     0u,
                                                                     returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList->reset();
    auto usedAfter = commandList->commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itor);
}
HWTEST_F(CommandListCreate, givenBindlessModeDisabledWhenCommandListsResetThenSbaReloaded) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(0);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::Compute,
                                                                     0u,
                                                                     returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList->reset();
    auto usedAfter = commandList->commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenResetThenStateBaseAddressNotProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    commandList->reset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWhenSetBarrierThenPipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    commandList->appendBarrier(nullptr, 0, nullptr);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenAppendingBarrierThenPipeControlIsProgrammedAndHdcFlushIsSet, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    returnValue = commandList->appendBarrier(nullptr, 0, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*itor);
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
}

HWTEST_F(CommandListCreate, givenCommandListWhenAppendingBarrierWithIncorrectWaitEventsThenInvalidArgumentIsReturned) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    returnValue = commandList->appendBarrier(nullptr, 4u, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

using Platforms = IsAtLeastProduct<IGFX_SKYLAKE>;

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingBeforeCommandForCopyOnlyThenCommandsHaveCorrectEventOffsets, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    auto baseAddr = event->getGpuAddress(device);
    auto contextOffset = event->getContextStartOffset();
    auto globalOffset = event->getGlobalStartOffset();
    EXPECT_EQ(baseAddr, event->getPacketAddress(device));

    commandList->appendEventForProfilingCopyCommand(event->toHandle(), true);
    EXPECT_EQ(1u, event->getPacketsInUse());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalOffset));
    EXPECT_NE(cmdList.end(), ++itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextOffset));
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingAfterCommandForCopyOnlyThenCommandsHaveCorrectEventOffsets, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    commandList->appendEventForProfilingCopyCommand(event->toHandle(), false);

    auto contextOffset = event->getContextEndOffset();
    auto globalOffset = event->getGlobalEndOffset();
    auto baseAddr = event->getGpuAddress(device);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalOffset));
    EXPECT_NE(cmdList.end(), ++itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextOffset));
}

HWTEST2_F(CommandListCreate, givenNullEventWhenAppendEventAfterWalkerThenNothingAddedToStream, Platforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);

    auto usedBefore = commandList->commandContainer.getCommandStream()->getUsed();

    commandList->appendSignalEventPostWalker(nullptr);

    EXPECT_EQ(commandList->commandContainer.getCommandStream()->getUsed(), usedBefore);
}

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/state_base_address_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl_timestamps.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device_for_spirv.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "test_traits_common.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;

HWTEST_F(CommandListCreate, givenCommandListWithInvalidWaitEventArgWhenAppendQueryKernelTimestampsThenProperErrorRetruned) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    device->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::queryKernelTimestamps);
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device, &deviceDesc, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto eventHandle = event.toHandle();

    result = commandList->appendQueryKernelTimestamps(1u, &eventHandle, alloc, nullptr, nullptr, 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    context->freeMem(alloc);
}

using AppendQueryKernelTimestamps = CommandListCreate;

HWTEST_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsWithoutOffsetsThenProperBuiltinWasAdded) {
    USE_REAL_FILE_SYSTEM();

    std::unique_ptr<MockDeviceForSpv<false, false>> testDevice = std::unique_ptr<MockDeviceForSpv<false, false>>(new MockDeviceForSpv<false, false>(device->getNEODevice(), driverHandle.get()));
    testDevice->builtins.reset(new MockBuiltinFunctionsLibImplTimestamps(testDevice.get(), testDevice->getNEODevice()->getBuiltIns()));
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::queryKernelTimestamps);
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::queryKernelTimestampsWithOffsets);

    device = testDevice.get();

    MockCommandListForAppendLaunchKernel<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto result = context->allocDeviceMem(device, &deviceDesc, 128, 1, &alloc);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    bool containsDstPtr = false;
    bool gpuTimeStampAlloc = false;
    for (auto &residentGfxAlloc : commandList.cmdListHelper.argumentsResidencyContainer) {
        if (residentGfxAlloc != nullptr) {
            if (residentGfxAlloc->getGpuAddress() ==
                reinterpret_cast<uint64_t>(alloc)) {
                containsDstPtr = true;
            }
            if (residentGfxAlloc->getAllocationType() ==
                NEO::AllocationType::gpuTimestampDeviceBuffer) {
                gpuTimeStampAlloc = true;
            }
        }
    }

    EXPECT_TRUE(containsDstPtr);
    EXPECT_TRUE(gpuTimeStampAlloc);

    EXPECT_EQ(testDevice->getBuiltinFunctionsLib()->getFunction(Builtin::queryKernelTimestamps)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());
    EXPECT_EQ(2u, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[2]);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(gfxCoreHelper.useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    EXPECT_TRUE(commandList.cmdListHelper.isBuiltin);
    EXPECT_FALSE(commandList.cmdListHelper.isDstInSystem);

    context->freeMem(alloc);
}

HWTEST_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsWithOffsetsThenProperBuiltinWasAdded) {
    USE_REAL_FILE_SYSTEM();

    std::unique_ptr<MockDeviceForSpv<false, false>> testDevice = std::unique_ptr<MockDeviceForSpv<false, false>>(new MockDeviceForSpv<false, false>(device->getNEODevice(), driverHandle.get()));
    testDevice->builtins.reset(new MockBuiltinFunctionsLibImplTimestamps(testDevice.get(), testDevice->getNEODevice()->getBuiltIns()));
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::queryKernelTimestamps);
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::queryKernelTimestampsWithOffsets);

    device = testDevice.get();

    MockCommandListForAppendLaunchKernel<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto result = context->allocDeviceMem(device, &deviceDesc, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    void *offsetAlloc;
    result = context->allocDeviceMem(device, &deviceDesc, 128, 1, &offsetAlloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};

    auto offsetSizes = reinterpret_cast<size_t *>(offsetAlloc);
    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, offsetSizes, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    bool containsDstPtr = false;

    for (auto &a : commandList.cmdListHelper.argumentsResidencyContainer) {
        if (a != nullptr && a->getGpuAddress() == reinterpret_cast<uint64_t>(alloc)) {
            containsDstPtr = true;
        }
    }

    EXPECT_TRUE(containsDstPtr);

    bool containOffsetPtr = false;

    for (auto &a : commandList.cmdListHelper.argumentsResidencyContainer) {
        if (a != nullptr && a->getGpuAddress() == reinterpret_cast<uint64_t>(offsetAlloc)) {
            containOffsetPtr = true;
        }
    }

    EXPECT_TRUE(containOffsetPtr);

    EXPECT_EQ(device->getBuiltinFunctionsLib()->getFunction(Builtin::queryKernelTimestampsWithOffsets)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());
    EXPECT_EQ(2u, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[2]);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(gfxCoreHelper.useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    context->freeMem(alloc);
    context->freeMem(offsetAlloc);
}

HWTEST_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsInUsmHostMemoryWithEventsNumberBiggerThanMaxWorkItemSizeThenProperGroupSizeAndGroupCountIsSet) {
    USE_REAL_FILE_SYSTEM();

    std::unique_ptr<MockDeviceForSpv<false, false>> testDevice = std::unique_ptr<MockDeviceForSpv<false, false>>(new MockDeviceForSpv<false, false>(device->getNEODevice(), driverHandle.get()));
    testDevice->builtins.reset(new MockBuiltinFunctionsLibImplTimestamps(testDevice.get(), testDevice->getNEODevice()->getBuiltIns()));
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::queryKernelTimestamps);

    device = testDevice.get();

    MockCommandListForAppendLaunchKernel<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));

    size_t eventCount = device->getNEODevice()->getDeviceInfo().maxWorkItemSizes[0] * 2u;
    std::unique_ptr<ze_event_handle_t[]> events = std::make_unique<ze_event_handle_t[]>(eventCount);

    for (size_t i = 0u; i < eventCount; ++i) {
        events[i] = event.toHandle();
    }

    void *alloc;
    ze_host_mem_alloc_desc_t hostDesc = {};
    size_t size = sizeof(ze_kernel_timestamp_result_t) * eventCount;
    auto result = context->allocHostMem(&hostDesc, size, 4096u, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList.appendQueryKernelTimestamps(static_cast<uint32_t>(eventCount), events.get(), alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(device->getBuiltinFunctionsLib()->getFunction(Builtin::queryKernelTimestamps)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());

    uint32_t groupSizeX = static_cast<uint32_t>(eventCount);
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    device->getBuiltinFunctionsLib()->getFunction(Builtin::queryKernelTimestamps)->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);

    EXPECT_EQ(groupSizeX, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(groupSizeY, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(groupSizeZ, commandList.cmdListHelper.groupSize[2]);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(gfxCoreHelper.useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(static_cast<uint32_t>(eventCount) / groupSizeX, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    EXPECT_TRUE(commandList.cmdListHelper.isBuiltin);
    EXPECT_TRUE(commandList.cmdListHelper.isDstInSystem);

    context->freeMem(alloc);
}

HWTEST_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsInExternalHostMemoryWithEventsNumberBiggerThanMaxWorkItemSizeThenProperGroupSizeAndGroupCountIsSet) {
    USE_REAL_FILE_SYSTEM();

    std::unique_ptr<MockDeviceForSpv<false, false>> testDevice = std::unique_ptr<MockDeviceForSpv<false, false>>(new MockDeviceForSpv<false, false>(device->getNEODevice(), driverHandle.get()));
    testDevice->builtins.reset(new MockBuiltinFunctionsLibImplTimestamps(testDevice.get(), testDevice->getNEODevice()->getBuiltIns()));
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::queryKernelTimestamps);

    device = testDevice.get();

    MockCommandListForAppendLaunchKernel<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));

    size_t eventCount = device->getNEODevice()->getDeviceInfo().maxWorkItemSizes[0] * 2u;
    std::unique_ptr<ze_event_handle_t[]> events = std::make_unique<ze_event_handle_t[]>(eventCount);

    for (size_t i = 0u; i < eventCount; ++i) {
        events[i] = event.toHandle();
    }

    size_t size = sizeof(ze_kernel_timestamp_result_t) * eventCount;
    auto alloc = std::make_unique<uint8_t[]>(size);

    auto result = commandList.appendQueryKernelTimestamps(static_cast<uint32_t>(eventCount), events.get(), alloc.get(), nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(device->getBuiltinFunctionsLib()->getFunction(Builtin::queryKernelTimestamps)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());

    uint32_t groupSizeX = static_cast<uint32_t>(eventCount);
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    device->getBuiltinFunctionsLib()->getFunction(Builtin::queryKernelTimestamps)->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);

    EXPECT_EQ(groupSizeX, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(groupSizeY, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(groupSizeZ, commandList.cmdListHelper.groupSize[2]);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(gfxCoreHelper.useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(static_cast<uint32_t>(eventCount) / groupSizeX, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    EXPECT_TRUE(commandList.cmdListHelper.isBuiltin);
    EXPECT_TRUE(commandList.cmdListHelper.isDstInSystem);
}

HWTEST_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsAndInvalidResultSuggestGroupSizeThenUnknownResultReturned) {
    class MockQueryKernelTimestampsKernel : public L0::KernelImp {
      public:
        ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY,
                                     uint32_t globalSizeZ, uint32_t *groupSizeX,
                                     uint32_t *groupSizeY, uint32_t *groupSizeZ) override {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
            return;
        }
        void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
            return;
        }

        uint32_t getIndirectSize() const override {
            return getCrossThreadDataSize() + getPerThreadDataSizeForWholeThreadGroup();
        }
    };
    struct MockBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {

        using BuiltinFunctionsLibImpl::builtins;
        using BuiltinFunctionsLibImpl::getFunction;
        using BuiltinFunctionsLibImpl::imageBuiltins;
        MockBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {}
    };
    struct MockBuiltinFunctionsForQueryKernelTimestamps : BuiltinFunctionsLibImpl {
        MockBuiltinFunctionsForQueryKernelTimestamps(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {
            tmpMockKernel = new MockQueryKernelTimestampsKernel;
        }
        MockQueryKernelTimestampsKernel *getFunction(Builtin func) override {
            return tmpMockKernel;
        }
        ~MockBuiltinFunctionsForQueryKernelTimestamps() override {
            delete tmpMockKernel;
        }
        MockQueryKernelTimestampsKernel *tmpMockKernel = nullptr;
    };
    class MockDeviceHandle : public L0::DeviceImp {
      public:
        MockDeviceHandle() {
        }
        void initialize(L0::Device *device) {
            neoDevice = device->getNEODevice();
            neoDevice->incRefInternal();
            driverHandle = device->getDriverHandle();
            tmpMockBultinLib = new MockBuiltinFunctionsForQueryKernelTimestamps{this, device->getNEODevice()->getBuiltIns()};
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *getBuiltinFunctionsLib() override {
            return tmpMockBultinLib;
        }
        ~MockDeviceHandle() override {
            delete tmpMockBultinLib;
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *tmpMockBultinLib = nullptr;
    };

    MockDeviceHandle mockDevice;
    mockDevice.initialize(device);

    MockCommandListForAppendLaunchKernel<FamilyType::gfxCoreFamily> commandList;

    commandList.initialize(&mockDevice, NEO::EngineGroupType::renderCompute, 0u);

    MockEvent event;
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(mockDevice.getRootDeviceIndex(), mockDevice.toHandle()));
    auto result = context->allocDeviceMem(&mockDevice, &deviceDesc, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    context->freeMem(alloc);
}

HWTEST_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsAndInvalidResultSetGroupSizeThenUnknownResultReturned) {
    class MockQueryKernelTimestampsKernel : public L0::KernelImp {
      public:
        ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY,
                                     uint32_t globalSizeZ, uint32_t *groupSizeX,
                                     uint32_t *groupSizeY, uint32_t *groupSizeZ) override {
            *groupSizeX = static_cast<uint32_t>(1u);
            *groupSizeY = static_cast<uint32_t>(1u);
            *groupSizeZ = static_cast<uint32_t>(1u);
            return ZE_RESULT_SUCCESS;
        }
        ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY,
                                 uint32_t groupSizeZ) override {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
            return;
        }
        void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
            return;
        }

        uint32_t getIndirectSize() const override {
            return getCrossThreadDataSize() + getPerThreadDataSizeForWholeThreadGroup();
        }
    };
    struct MockBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {

        using BuiltinFunctionsLibImpl::builtins;
        using BuiltinFunctionsLibImpl::getFunction;
        using BuiltinFunctionsLibImpl::imageBuiltins;
        MockBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {}
    };
    struct MockBuiltinFunctionsForQueryKernelTimestamps : BuiltinFunctionsLibImpl {
        MockBuiltinFunctionsForQueryKernelTimestamps(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {
            tmpMockKernel = new MockQueryKernelTimestampsKernel;
        }
        MockQueryKernelTimestampsKernel *getFunction(Builtin func) override {
            return tmpMockKernel;
        }
        ~MockBuiltinFunctionsForQueryKernelTimestamps() override {
            delete tmpMockKernel;
        }
        MockQueryKernelTimestampsKernel *tmpMockKernel = nullptr;
    };
    class MockDeviceHandle : public L0::DeviceImp {
      public:
        MockDeviceHandle() {
        }
        void initialize(L0::Device *device) {
            neoDevice = device->getNEODevice();
            neoDevice->incRefInternal();
            driverHandle = device->getDriverHandle();
            tmpMockBultinLib = new MockBuiltinFunctionsForQueryKernelTimestamps{this, device->getNEODevice()->getBuiltIns()};
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *getBuiltinFunctionsLib() override {
            return tmpMockBultinLib;
        }
        ~MockDeviceHandle() override {
            delete tmpMockBultinLib;
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *tmpMockBultinLib = nullptr;
    };

    MockDeviceHandle mockDevice;
    mockDevice.initialize(device);

    MockCommandListForAppendLaunchKernel<FamilyType::gfxCoreFamily> commandList;

    commandList.initialize(&mockDevice, NEO::EngineGroupType::renderCompute, 0u);

    MockEvent event;
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(mockDevice.getRootDeviceIndex(), mockDevice.toHandle()));
    auto result = context->allocDeviceMem(&mockDevice, &deviceDesc, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    context->freeMem(alloc);
}

HWTEST_F(AppendQueryKernelTimestamps, givenEventWhenAppendQueryIsCalledThenSetAllEventData) {
    class MockQueryKernelTimestampsKernel : public L0::KernelImp {
      public:
        MockQueryKernelTimestampsKernel(MockModule *module) : KernelImp{module}, module{module} {
            mockKernelImmutableData.kernelDescriptor = &mockKernelDescriptor;
            size_t stubKernelHeapSize = 42;
            mockKernelImmutableData.setIsaPerKernelAllocation(module->allocateKernelsIsaMemory(stubKernelHeapSize));
            this->sharedState->kernelImmData = &mockKernelImmutableData;
        }

        ~MockQueryKernelTimestampsKernel() override {
            this->module->getDevice()->getNEODevice()->getMemoryManager()->freeGraphicsMemory(mockKernelImmutableData.isaGraphicsAllocation.release());
        }

        ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation, NEO::SvmAllocationData *peerAllocData) override {
            if (argIndex == 0) {
                index0Allocation = allocation;
            }

            return ZE_RESULT_SUCCESS;
        }

        void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
            return;
        }
        void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
            return;
        }

        uint32_t getIndirectSize() const override {
            return getCrossThreadDataSize() + getPerThreadDataSizeForWholeThreadGroup();
        }

        NEO::GraphicsAllocation *index0Allocation = nullptr;
        KernelDescriptor mockKernelDescriptor = {};
        WhiteBox<::L0::KernelImmutableData> mockKernelImmutableData = {};
        MockModule *module = nullptr;
    };

    struct MockBuiltinFunctionsForQueryKernelTimestamps : BuiltinFunctionsLibImpl {
        MockBuiltinFunctionsForQueryKernelTimestamps(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {
            tmpModule = std::make_unique<MockModule>(device, nullptr, ModuleType::builtin);
            tmpMockKernel = std::make_unique<MockQueryKernelTimestampsKernel>(tmpModule.get());
        }
        MockQueryKernelTimestampsKernel *getFunction(Builtin func) override {
            return tmpMockKernel.get();
        }

        std::unique_ptr<MockModule> tmpModule;
        std::unique_ptr<MockQueryKernelTimestampsKernel> tmpMockKernel;
    };
    class MockDeviceHandle : public L0::DeviceImp {
      public:
        MockDeviceHandle() {
        }
        void initialize(L0::Device *device) {
            neoDevice = device->getNEODevice();
            neoDevice->incRefInternal();
            driverHandle = device->getDriverHandle();
            tmpMockBultinLib = std::make_unique<MockBuiltinFunctionsForQueryKernelTimestamps>(this, device->getNEODevice()->getBuiltIns());
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *getBuiltinFunctionsLib() override {
            return tmpMockBultinLib.get();
        }
        std::unique_ptr<MockBuiltinFunctionsForQueryKernelTimestamps> tmpMockBultinLib;
    };

    MockDeviceHandle mockDevice;
    mockDevice.initialize(device);

    MockCommandListForAppendLaunchKernel<FamilyType::gfxCoreFamily> commandList;

    commandList.initialize(&mockDevice, NEO::EngineGroupType::renderCompute, 0u);

    MockEvent event;
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(mockDevice.getRootDeviceIndex(), mockDevice.toHandle()));
    auto result = context->allocDeviceMem(&mockDevice, &deviceDesc, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto index0Allocation = mockDevice.tmpMockBultinLib->tmpMockKernel->index0Allocation;
    EXPECT_NE(nullptr, index0Allocation);

    EventData *eventData = reinterpret_cast<EventData *>(index0Allocation->getUnderlyingBuffer());

    EXPECT_EQ(eventData[0].address, event.getGpuAddress(&mockDevice));
    EXPECT_EQ(eventData[0].packetsInUse, event.getPacketsInUse());
    EXPECT_EQ(eventData[0].timestampSizeInDw, event.getTimestampSizeInDw());

    EXPECT_EQ(eventData[1].address, event.getGpuAddress(&mockDevice));
    EXPECT_EQ(eventData[1].packetsInUse, event.getPacketsInUse());
    EXPECT_EQ(eventData[1].timestampSizeInDw, event.getTimestampSizeInDw());

    context->freeMem(alloc);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenAppendSignalEventThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    commandList->appendSignalEvent(event.toHandle(), false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWhenAppendSignalEventWithScopeThenPipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    commandList->appendSignalEvent(event.toHandle(), false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

using CommandListTimestampEvent = Test<DeviceFixture>;

HWTEST_F(CommandListTimestampEvent, WhenIsTimestampEventForMultiTileThenCorrectResultIsReturned) {

    auto cmdList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();

    cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    MockEvent mockEvent;

    cmdList->partitionCount = 1u;
    EXPECT_FALSE(cmdList->isTimestampEventForMultiTile(nullptr));

    cmdList->partitionCount = 2u;
    EXPECT_FALSE(cmdList->isTimestampEventForMultiTile(nullptr));

    mockEvent.setEventTimestampFlag(false);
    EXPECT_FALSE(cmdList->isTimestampEventForMultiTile(&mockEvent));

    mockEvent.setEventTimestampFlag(true);
    EXPECT_TRUE(cmdList->isTimestampEventForMultiTile(&mockEvent));
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenAppendWaitEventsWithDcFlushThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    auto eventHandle = event.toHandle();
    commandList->appendWaitOnEvents(1, &eventHandle, nullptr, false, true, false, false, false, false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    if (MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment())) {
        EXPECT_NE(cmdList.end(), itor);
    } else {
        EXPECT_EQ(cmdList.end(), itor);
    }
}

HWTEST_F(CommandListCreate, givenCommandListyWhenAppendWaitEventsWithDcFlushThenPipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    auto eventHandle = event.toHandle();
    commandList->appendWaitOnEvents(1, &eventHandle, nullptr, false, true, false, false, false, false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment())) {
        itor--;
        EXPECT_NE(nullptr, genCmdCast<PIPE_CONTROL *>(*itor));
    } else {
        if (cmdList.begin() != itor) {
            itor--;
            EXPECT_EQ(nullptr, genCmdCast<PIPE_CONTROL *>(*itor));
        }
    }
}

HWTEST_F(CommandListCreate, givenCommandListWhenAppendWaitEventsWithDcFlushThenPipeControlIsProgrammedOnlyOnce) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event2.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    ze_event_handle_t events[] = {&event, &event2};

    commandList->appendWaitOnEvents(2, events, nullptr, false, true, false, false, false, false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()) && !whiteBoxCmdList->l3FlushAfterPostSyncEnabled) {
        itor--;
        EXPECT_NE(nullptr, genCmdCast<PIPE_CONTROL *>(*itor));
    } else {
        if (cmdList.begin() != itor) {
            itor--;
            EXPECT_EQ(nullptr, genCmdCast<PIPE_CONTROL *>(*itor));
        }
    }
}

HWTEST_F(CommandListCreate, givenAsyncCmdQueueAndImmediateCommandListWhenAppendWaitEventsWithHostScopeThenPipeControlAndSemWaitAreAddedFromCommandList) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.SignalAllEventPackets.set(0);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    size_t expectedUsed = 2 * NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() + sizeof(MI_BATCH_BUFFER_END);
    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()) && !whiteBoxCmdList->l3FlushAfterPostSyncEnabled) {
        expectedUsed += sizeof(PIPE_CONTROL);
    }
    expectedUsed = alignUp(expectedUsed, 64);

    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event2.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    ze_event_handle_t events[] = {&event, &event2};

    size_t startOffset = commandContainer.getCommandStream()->getUsed();
    commandList->appendWaitOnEvents(2, events, nullptr, false, true, false, false, false, false);
    size_t endOffset = commandContainer.getCommandStream()->getUsed();

    size_t usedBufferSize = (endOffset - startOffset);
    EXPECT_EQ(expectedUsed, usedBufferSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), startOffset),
        expectedUsed));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    EXPECT_EQ(expectedUsed, commandContainer.getCommandStream()->getUsed());
}

HWTEST_F(CommandListCreate, givenAsyncCmdQueueAndImmediateCommandListWhenAppendWaitEventsWithSubdeviceScopeThenSemWaitIsAddedFromCommandList) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.SignalAllEventPackets.set(0);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    size_t expectedUsed = 2 * NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() + sizeof(MI_BATCH_BUFFER_END);
    expectedUsed = alignUp(expectedUsed, 64);

    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = 0;
    event2.waitScope = 0;
    ze_event_handle_t events[] = {&event, &event2};

    size_t startOffset = commandContainer.getCommandStream()->getUsed();
    commandList->appendWaitOnEvents(2, events, nullptr, false, true, false, false, false, false);
    size_t endOffset = commandContainer.getCommandStream()->getUsed();

    size_t usedBufferSize = (endOffset - startOffset);
    EXPECT_EQ(expectedUsed, usedBufferSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), startOffset),
        expectedUsed));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    EXPECT_EQ(expectedUsed, commandContainer.getCommandStream()->getUsed());
}

HWTEST_F(CommandListCreate, givenFlushTaskFlagEnabledAndAsyncCmdQueueAndCopyOnlyImmediateCommandListWhenAppendWaitEventsWithHostScopeThenMiFlushAndSemWaitAreAdded) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event2.waitScope = 0;
    ze_event_handle_t events[] = {&event, &event2};

    auto used = commandContainer.getCommandStream()->getUsed();
    commandList->appendWaitOnEvents(2, events, nullptr, false, true, false, false, false, false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    EXPECT_GT(commandContainer.getCommandStream()->getUsed(), used);
}

HWTEST_F(CommandListCreate, givenImmediateCommandListAndAlreadyCompletedEventWhenAddEventsToCmdListThenProgramSemaphoresOnlyForIncompletedEvents) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.SignalAllEventPackets.set(0);
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event2.waitScope = 0;
    ze_event_handle_t events[] = {&event, &event2};
    event.isCompleted = Event::State::STATE_SIGNALED;

    static_cast<CommandListCoreFamily<FamilyType::gfxCoreFamily> *>(commandList.get())->addEventsToCmdList(2, events, nullptr, false, false, true, false, false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor++);
    itor = find<SEMAPHORE_WAIT *>(itor, cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

struct CmdContainerMock : public CommandContainer {
    using CommandContainer::secondaryCommandStreamForImmediateCmdList;
};

HWTEST_F(CommandListCreate, givenImmediateCopyOnlySingleTileDirectSubmissionCommandListWhenInitializeThenCreateSecondaryCmdBufferInSystemMemory) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, desc.ordinal, desc.index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    VariableBackup<bool> directSubmissionAvailable{&reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable, true};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto flatRingSupported = device->getHwInfo().featureTable.flags.ftrLocalMemory;
    EXPECT_EQ(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get() != nullptr, flatRingSupported);
    if (flatRingSupported) {
        EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList->getGraphicsAllocation()->getMemoryPool()));
    }
}

HWTEST_F(CommandListCreate, givenMetricsImmediateCopyOnlySingleTileDirectSubmissionCommandListWhenInitializeThenNotCreateSecondaryCmdBufferInSystemMemory) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    device->getNEODevice()->getExecutionEnvironment()->setMetricsEnabled(true);
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, desc.ordinal, desc.index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    VariableBackup<bool> directSubmissionAvailable{&reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable, true};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get(), nullptr);
}

HWTEST_F(CommandListCreate, givenSecondaryCommandStreamForImmediateCmdListWhenCheckAvailableSpaceThenSwapCommandStreams) {
    if (!device->getHwInfo().featureTable.flags.ftrLocalMemory) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    static_cast<MockMemoryManager *>(device->getNEODevice()->getMemoryManager())->localMemorySupported[0] = true;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, desc.ordinal, desc.index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    VariableBackup<bool> directSubmissionAvailable{&reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable, true};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_NE(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get(), nullptr);
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList->getGraphicsAllocation()->getMemoryPool()));

    auto immediateCmdList = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get());
    auto secondaryCmdStream = reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get();

    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, false);

    EXPECT_EQ(commandList->getCmdContainer().getCommandStream(), secondaryCmdStream);
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));

    commandList->reset();

    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList->getGraphicsAllocation()->getMemoryPool()));
}

HWTEST2_F(CommandListCreate, givenSystemAndLocalCommandStreamForImmediateCmdListWhenLocalIsRequiredAtCheckAvailableSpaceThenSwapCommandStreams, IsAtLeastXeHpcCore) {
    auto mutableHwInfo = device->getNEODevice()->getRootDeviceEnvironmentRef().getMutableHardwareInfo();
    VariableBackup<NEO::HardwareInfo> backupHwInfo(mutableHwInfo);
    mutableHwInfo->featureTable.flags.ftrLocalMemory = true;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(1);

    static_cast<MockMemoryManager *>(device->getNEODevice()->getMemoryManager())->localMemorySupported[0] = true;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_NE(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get(), nullptr);
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList->getGraphicsAllocation()->getMemoryPool()));

    auto immediateCmdList = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get());
    auto primaryCmdStream = commandList->getCmdContainer().getCommandStream();

    // make sure system is current
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, false);

    // switch to local
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, true);

    // expect primary
    EXPECT_EQ(commandList->getCmdContainer().getCommandStream(), primaryCmdStream);
    // primary in local
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));

    // exhaust primary local
    primaryCmdStream->getSpace(primaryCmdStream->getAvailableSpace() - 4);

    // check correct allocation is refreshed
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, true);
    EXPECT_EQ(primaryCmdStream->getAvailableSpace(), primaryCmdStream->getMaxAvailableSpace());

    // check still in local
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));

    // exhaust local but switch to system
    primaryCmdStream->getSpace(primaryCmdStream->getAvailableSpace() - 4);
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, false);
    // current in system
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));

    // ask for local - verify new allocation is created
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, true);
    EXPECT_EQ(primaryCmdStream->getAvailableSpace(), primaryCmdStream->getMaxAvailableSpace());
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));
}

HWTEST2_F(CommandListCreate, givenSystemAndLocalCommandStreamForImmediateCmdListWhenSystemIsRequiredAtCheckAvailableSpaceThenSwapCommandStreams, IsAtLeastXeHpcCore) {
    auto mutableHwInfo = device->getNEODevice()->getRootDeviceEnvironmentRef().getMutableHardwareInfo();
    VariableBackup<NEO::HardwareInfo> backupHwInfo(mutableHwInfo);
    mutableHwInfo->featureTable.flags.ftrLocalMemory = true;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(1);

    static_cast<MockMemoryManager *>(device->getNEODevice()->getMemoryManager())->localMemorySupported[0] = true;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::compute, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_NE(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get(), nullptr);
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList->getGraphicsAllocation()->getMemoryPool()));

    auto immediateCmdList = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get());
    auto secondaryCmdStream = reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get();

    // make sure local is current
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, true);

    // switch to system
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, false);

    // expect secondary
    EXPECT_EQ(commandList->getCmdContainer().getCommandStream(), secondaryCmdStream);
    // secondary in system
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));

    // exhaust system
    secondaryCmdStream->getSpace(secondaryCmdStream->getAvailableSpace() - 4);

    // check correct allocation is refreshed
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, false);
    EXPECT_EQ(secondaryCmdStream->getAvailableSpace(), secondaryCmdStream->getMaxAvailableSpace());

    // still in system
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));

    // exhaust system but switch to local
    secondaryCmdStream->getSpace(secondaryCmdStream->getAvailableSpace() - 4);
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, true);
    // current in local
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));

    // ask for system - verify new allocation is created
    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, false);
    EXPECT_EQ(secondaryCmdStream->getAvailableSpace(), secondaryCmdStream->getMaxAvailableSpace());
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));
}

HWTEST_F(CommandListCreate, givenNoSecondaryCommandStreamForImmediateCmdListWhenCheckAvailableSpaceThenNotSwapCommandStreams) {
    if (!device->getHwInfo().featureTable.flags.ftrLocalMemory) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);

    static_cast<MockMemoryManager *>(device->getNEODevice()->getMemoryManager())->localMemorySupported[0] = true;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get(), nullptr);

    auto immediateCmdList = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(commandList.get());
    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    immediateCmdList->checkAvailableSpace(0u, false, commonImmediateCommandSize, false);

    EXPECT_EQ(commandList->getCmdContainer().getCommandStream(), cmdStream);
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation()->getMemoryPool()));
}

HWTEST_F(CommandListCreate, givenDirectSubmissionFlatRingBufferFlagDisabledImmediateCopyOnlySingleTileDirectSubmissionCommandListWhenInitializeThenNotCreateSecondaryCmdBufferInSystemMemory) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(0u);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, desc.ordinal, desc.index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    VariableBackup<bool> directSubmissionAvailable{&reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable, true};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get(), nullptr);
}

HWTEST_F(CommandListCreate, givenImmediateCopyOnlySingleTileCommandListWhenInitializeThenNotCreateSecondaryCmdBufferInSystemMemory) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get(), nullptr);
}

using CommandListCreateImplicitScaling = Test<SingleRootMultiSubDeviceFixtureWithImplicitScaling<1u, 1u>>;
HWTEST_F(CommandListCreateImplicitScaling, givenImmediateCopyOnlyDirectSubmissionCommandListWhenInitializeThenNotCreateSecondaryCmdBufferInSystemMemory) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, desc.ordinal, desc.index, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    VariableBackup<bool> directSubmissionAvailable{&reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable, true};
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get(), nullptr);
}

HWTEST_F(CommandListCreate, givenCopyOnlySingleTileDirectSubmissionCommandListWhenInitializeThenNotCreateSecondaryCmdBufferInSystemMemory) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionFlatRingBuffer.set(-1);
    ze_result_t returnValue;
    CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    VariableBackup<bool> directSubmissionAvailable{&reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable, true};
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(reinterpret_cast<CmdContainerMock *>(&commandList->getCmdContainer())->secondaryCommandStreamForImmediateCmdList.get(), nullptr);
}

HWTEST_F(CommandListCreate, givenAsyncCmdQueueAndCopyOnlyImmediateCommandListWhenAppendWaitEventsWithSubdeviceScopeThenMiFlushAndSemWaitAreAdded) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    auto &commandContainer = commandList->getCmdContainer();
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = 0;
    event2.waitScope = 0;
    ze_event_handle_t events[] = {&event, &event2};

    auto used = commandContainer.getCommandStream()->getUsed();
    commandList->appendWaitOnEvents(2, events, nullptr, false, true, false, false, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    EXPECT_GT(commandContainer.getCommandStream()->getUsed(), used);
}

HWTEST_F(CommandListCreate, givenAsyncCmdQueueAndTbxCsrWithCopyOnlyImmediateCommandListWhenAppendWaitEventsReturnsSuccess) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    whiteBoxCmdList->isTbxMode = true;

    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = 0;
    event2.waitScope = 0;
    ze_event_handle_t events[] = {&event, &event2};

    auto ret = commandList->appendWaitOnEvents(2, events, nullptr, false, true, false, false, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

using CommandListStateBaseAddressPrivateHeapTest = Test<CommandListPrivateHeapsFixture>;

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenRegularCmdListAppendKernelAndExecuteThenBaseAddressStateIsStoredInCsr,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    checkAndPrepareBindlessKernel();

    StackVec<size_t, 4> sbaCmdsSizes = {};
    sbaCmdsSizes = {0, expectedSbaCmds, 0, 0};

    auto sbaCmdsSizesIter = sbaCmdsSizes.begin();

    NEO::StateBaseAddressPropertiesSupport sbaPropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillStateBaseAddressPropertiesSupportStructure(sbaPropertiesSupport);

    EXPECT_TRUE(commandList->stateBaseAddressTracking);

    auto &container = commandList->getCmdContainer();
    auto &cmdListStream = *container.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        cmdListStream.getUsed()));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    uint64_t ssBaseAddress = -1;
    uint64_t ssSize = -1;
    if (!bindlessHeapsHelper) {
        ssBaseAddress = NEO::getStateBaseAddressForSsh(*sshHeap, bindlessHeapsHelper);
        ssSize = NEO::getStateSizeForSsh(*sshHeap, bindlessHeapsHelper);
    }

    uint64_t dsBaseAddress = -1;
    size_t dsSize = static_cast<size_t>(-1);

    auto dshHeap = container.getIndirectHeap(NEO::HeapType::dynamicState);
    if (NEO::UnitTestHelper<FamilyType>::expectNullDsh(device->getDeviceInfo())) {
        EXPECT_EQ(nullptr, dshHeap);
    } else {
        EXPECT_NE(nullptr, dshHeap);
    }

    if (dshHeap) {
        dsBaseAddress = NEO::getStateBaseAddress(*dshHeap, bindlessHeapsHelper);
        dsSize = NEO::getStateSize(*dshHeap, bindlessHeapsHelper);
    }

    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto statlessMocs = getMocs(true);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredState.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, requiredState.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSize, requiredState.dynamicStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredState.indirectObjectSize.value);

    if (sbaPropertiesSupport.bindingTablePoolBaseAddress) {
        EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), requiredState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(ssSize, requiredState.bindingTablePoolSize.value);
    } else {
        EXPECT_EQ(-1, requiredState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), requiredState.bindingTablePoolSize.value);
    }

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.indirectObjectBaseAddress.value, requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalState.indirectObjectSize.value, requiredState.indirectObjectSize.value);

    EXPECT_EQ(finalState.bindingTablePoolBaseAddress.value, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalState.bindingTablePoolSize.value, requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.statelessMocs.value, requiredState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSize, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }
    if (bindlessHeapsHelper) {
        EXPECT_FALSE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getSurfaceStateBaseAddress());
    } else {
        EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());
    }

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    EXPECT_EQ(csrState.surfaceStateBaseAddress.value, finalState.surfaceStateBaseAddress.value);
    EXPECT_EQ(csrState.surfaceStateSize.value, finalState.surfaceStateSize.value);

    EXPECT_EQ(csrState.dynamicStateBaseAddress.value, finalState.dynamicStateBaseAddress.value);
    EXPECT_EQ(csrState.dynamicStateSize.value, finalState.dynamicStateSize.value);

    EXPECT_EQ(csrState.indirectObjectBaseAddress.value, finalState.indirectObjectBaseAddress.value);
    EXPECT_EQ(csrState.indirectObjectSize.value, finalState.indirectObjectSize.value);

    EXPECT_EQ(csrState.bindingTablePoolBaseAddress.value, finalState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(csrState.bindingTablePoolSize.value, finalState.bindingTablePoolSize.value);

    EXPECT_EQ(csrState.statelessMocs.value, finalState.statelessMocs.value);

    queueBefore = cmdQueueStream.getUsed();
    cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    result = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t afterReset = cmdListStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        afterReset));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenRegularCmdListAppendKernelChangesHeapsAndExecuteThenFinalBaseAddressStateIsStoredInCsr,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    checkAndPrepareBindlessKernel();

    StackVec<size_t, 4> sbaCmdsSizes = {};
    if (bindlessHeapsHelper) {
        sbaCmdsSizes = {0, 1, 1, 0};
    } else {
        sbaCmdsSizes = {0, expectedSbaCmds, expectedSbaCmds, expectedSbaCmds};
    }
    auto sbaCmdsSizesIter = sbaCmdsSizes.begin();

    NEO::StateBaseAddressPropertiesSupport sbaPropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillStateBaseAddressPropertiesSupportStructure(sbaPropertiesSupport);

    EXPECT_TRUE(commandList->stateBaseAddressTracking);

    auto &container = commandList->getCmdContainer();
    auto &cmdListStream = *container.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        cmdListStream.getUsed()));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    uint64_t ssBaseAddress = -1;
    uint64_t ssSize = -1;
    if (!bindlessHeapsHelper) {
        ssBaseAddress = NEO::getStateBaseAddressForSsh(*sshHeap, bindlessHeapsHelper);
        ssSize = NEO::getStateSizeForSsh(*sshHeap, bindlessHeapsHelper);
    }

    uint64_t dsBaseAddress = -1;
    uint32_t dsBaseSize = 0;
    size_t dsFirstBaseSize = 0;

    size_t dsSize = static_cast<size_t>(-1);

    auto dshHeap = container.getIndirectHeap(NEO::HeapType::dynamicState);
    if (dshHeap) {
        dsBaseAddress = NEO::getStateBaseAddress(*dshHeap, bindlessHeapsHelper);
        dsSize = dsFirstBaseSize = NEO::getStateSize(*dshHeap, bindlessHeapsHelper);
    }
    auto statlessMocs = getMocs(true);

    uint64_t ssFirstBaseAddress = ssBaseAddress == static_cast<uint64_t>(-1) ? 0u : ssBaseAddress;
    uint64_t dsFirstBaseAddress = dsBaseAddress;

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, requiredState.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSize, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    if (!bindlessHeapsHelper) {
        sshHeap->getSpace(sshHeap->getAvailableSpace());
        container.getHeapWithRequiredSizeAndAlignment(NEO::HeapType::surfaceState, sshHeap->getMaxAvailableSpace(), 0);
    }

    if (dshHeap) {
        dshHeap->getSpace(dshHeap->getAvailableSpace());
        container.getHeapWithRequiredSizeAndAlignment(NEO::HeapType::dynamicState, dshHeap->getMaxAvailableSpace(), 0);
    }

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (dshHeap) {
        dsBaseAddress = NEO::getStateBaseAddress(*dshHeap, bindlessHeapsHelper);
        dsBaseSize = static_cast<uint32_t>(NEO::getStateSize(*dshHeap, bindlessHeapsHelper));
    }

    if (!bindlessHeapsHelper) {
        ssBaseAddress = NEO::getStateBaseAddressForSsh(*sshHeap, bindlessHeapsHelper);
        EXPECT_NE(static_cast<int64_t>(ssBaseAddress), requiredState.surfaceStateBaseAddress.value);
    }

    if (dshHeap && !bindlessHeapsHelper) {
        EXPECT_NE(static_cast<int64_t>(dsBaseAddress), requiredState.dynamicStateBaseAddress.value);
    } else {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), requiredState.dynamicStateBaseAddress.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), finalState.surfaceStateBaseAddress.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), finalState.dynamicStateBaseAddress.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        cmdListStream.getUsed()));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds.back());

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsBaseSize, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    if (!bindlessHeapsHelper) {
        EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(sshHeap->getHeapGpuBase(), sbaCmd->getSurfaceStateBaseAddress());
    } else {
        EXPECT_FALSE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getSurfaceStateBaseAddress());
    }

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    EXPECT_EQ(csrState.surfaceStateBaseAddress.value, finalState.surfaceStateBaseAddress.value);
    EXPECT_EQ(csrState.surfaceStateSize.value, finalState.surfaceStateSize.value);

    EXPECT_EQ(csrState.dynamicStateBaseAddress.value, finalState.dynamicStateBaseAddress.value);
    EXPECT_EQ(csrState.dynamicStateSize.value, finalState.dynamicStateSize.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds.back());

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsFirstBaseAddress, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsFirstBaseSize, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    if (!bindlessHeapsHelper) {
        EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    }
    EXPECT_EQ(ssFirstBaseAddress, sbaCmd->getSurfaceStateBaseAddress());
    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    queueBefore = cmdQueueStream.getUsed();
    cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    EXPECT_FALSE(finalState.isDirty());

    if (!bindlessHeapsHelper) {
        ASSERT_EQ(1u, sbaCmds.size());

        sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds.back());

        if (this->dshRequired) {
            EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
            EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
            EXPECT_EQ(dsFirstBaseAddress, sbaCmd->getDynamicStateBaseAddress());
            EXPECT_EQ(dsFirstBaseSize, sbaCmd->getDynamicStateBufferSize());
        } else {
            EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
            EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
            EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
            EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
        }

        EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(ssFirstBaseAddress, sbaCmd->getSurfaceStateBaseAddress());
        EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
    }
}

struct CommandListPrivateHeapsBindlessSshFixture : public CommandListPrivateHeapsFixture {
    void setUp() {
        UnitTestSetter::disableHeapless(restorer);
        debugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
        debugManager.flags.UseBindlessMode.set(true);
        CommandListPrivateHeapsFixture::setUp();
    }

    DebugManagerStateRestore restorer;
};

using CommandListBindlessSshPrivateHeapTest = Test<CommandListPrivateHeapsBindlessSshFixture>;

HWTEST2_F(CommandListBindlessSshPrivateHeapTest,
          givenStateBaseAddressTrackingAndGlobalBindlessEnabledWhenRegularCmdListAppendKernelChangesHeapsAndExecuteThenBindlessSurfaceStateBaseIsProgrammedToGlobalBase,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto mockHelper = std::make_unique<MockBindlesHeapsHelper>(device->getNEODevice(),
                                                               device->getNEODevice()->getNumGenericSubDevices() > 1);
    mockHelper->globalBindlessDsh = false;
    auto globalBindlessBase = mockHelper->getGlobalHeapsBase();
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.reset(mockHelper.release());

    EXPECT_TRUE(commandList->stateBaseAddressTracking);

    auto &container = commandList->getCmdContainer();
    auto &cmdListStream = *container.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        cmdListStream.getUsed()));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    uint64_t ssBaseAddress = sshHeap->getHeapGpuBase();
    auto dshHeap = container.getIndirectHeap(NEO::HeapType::dynamicState);

    uint64_t ssFirstBaseAddress = ssBaseAddress;

    sshHeap->getSpace(sshHeap->getAvailableSpace());
    container.getHeapWithRequiredSizeAndAlignment(NEO::HeapType::surfaceState, sshHeap->getMaxAvailableSpace(), 0);

    if (dshHeap) {
        dshHeap->getSpace(dshHeap->getAvailableSpace());
        container.getHeapWithRequiredSizeAndAlignment(NEO::HeapType::dynamicState, dshHeap->getMaxAvailableSpace(), 0);
    }

    size_t cmdListBefore = cmdListStream.getUsed();

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ssBaseAddress = sshHeap->getGpuBase();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream.getCpuBase(), cmdListBefore),
        cmdListStream.getUsed() - cmdListBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(globalBindlessBase, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_NE(ssBaseAddress, sbaCmd->getBindlessSurfaceStateBaseAddress());
    EXPECT_EQ(globalBindlessBase, sbaCmd->getBindlessSurfaceStateBaseAddress());

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(globalBindlessBase, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_NE(ssFirstBaseAddress, sbaCmd->getBindlessSurfaceStateBaseAddress());
    EXPECT_EQ(globalBindlessBase, sbaCmd->getBindlessSurfaceStateBaseAddress());

    queueBefore = cmdQueueStream.getUsed();
    cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_NE(ssFirstBaseAddress, sbaCmd->getBindlessSurfaceStateBaseAddress());
    EXPECT_EQ(globalBindlessBase, sbaCmd->getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(CommandListBindlessSshPrivateHeapTest,
          givenStateBaseAddressTrackingAndGlobalBindlessSshAndDshEnabledWhenRegularCmdListExecutedThenBindlessSurfaceStateAndDynamicStateBaseIsProgrammedToGlobalBase,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto mockHelper = std::make_unique<MockBindlesHeapsHelper>(device->getNEODevice(),
                                                               device->getNEODevice()->getNumGenericSubDevices() > 1);
    mockHelper->globalBindlessDsh = true;
    auto globalBindlessBase = mockHelper->getGlobalHeapsBase();
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.reset(mockHelper.release());

    EXPECT_TRUE(commandList->stateBaseAddressTracking);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    memset(kernel->privateState.dynamicStateHeapData.data(), 0, kernel->privateState.dynamicStateHeapData.size());
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(globalBindlessBase, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(globalBindlessBase, sbaCmd->getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(CommandListBindlessSshPrivateHeapTest,
          givenBindlessKernelStateBaseAddressTrackingAndGlobalBindlessEnabledWhenRegularCmdListAppendsKernelThenReservedSshSizeIsZeroAndSbaIsNotAdded,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto mockHelper = std::make_unique<MockBindlesHeapsHelper>(device->getNEODevice(),
                                                               device->getNEODevice()->getNumGenericSubDevices() > 1);
    mockHelper->globalBindlessDsh = false;
    auto globalBindlessBase = mockHelper->getGlobalHeapsBase();
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.reset(mockHelper.release());

    EXPECT_TRUE(commandList->stateBaseAddressTracking);

    auto &container = commandList->getCmdContainer();
    auto &cmdListStream = *container.getCommandStream();

    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
    mockKernel.privateState.crossThreadData.resize(4 * sizeof(uint64_t), 0x0);
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
    mockKernel.descriptor.initBindlessOffsetToSurfaceState();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(mockKernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        cmdListStream.getUsed()));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(0u, sbaCmds.size());

    auto sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_EQ(nullptr, sshHeap);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;
    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_TRUE(sbaCmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(globalBindlessBase, sbaCmd->getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(CommandListBindlessSshPrivateHeapTest,
          givenBindlessKernelStateBaseAddressTrackingAndGlobalBindlessEnabledWhenOneArgUsesKernelsSshThenReservedSshSizeIsNonZero,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto mockHelper = std::make_unique<MockBindlesHeapsHelper>(device->getNEODevice(),
                                                               device->getNEODevice()->getNumGenericSubDevices() > 1);
    mockHelper->globalBindlessDsh = false;
    auto globalBindlessBase = mockHelper->getGlobalHeapsBase();
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.reset(mockHelper.release());

    EXPECT_TRUE(commandList->stateBaseAddressTracking);

    auto &container = commandList->getCmdContainer();
    auto &cmdListStream = *container.getCommandStream();

    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;

    mockKernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    mockKernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
    mockKernel.privateState.crossThreadData.resize(4 * sizeof(uint64_t), 0x0);
    const auto surfStateSize = static_cast<uint32_t>(device->getNEODevice()->getGfxCoreHelper().getRenderSurfaceStateSize());
    mockKernel.privateState.surfaceStateHeapData.resize(surfStateSize);
    mockKernel.info.heapInfo.surfaceStateHeapSize = surfStateSize;
    mockKernel.descriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
    mockKernel.descriptor.initBindlessOffsetToSurfaceState();
    mockKernel.privateState.usingSurfaceStateHeap.resize(1, false);
    mockKernel.privateState.isBindlessOffsetSet.resize(1, false);
    mockKernel.privateState.usingSurfaceStateHeap[0] = true;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(mockKernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        cmdListStream.getUsed()));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, sbaCmds.size());

    auto sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_NE(nullptr, sshHeap);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;
    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_TRUE(sbaCmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(globalBindlessBase, sbaCmd->getBindlessSurfaceStateBaseAddress());

    auto offsetInHeap = ptrDiff(sshHeap->getSpace(0), sshHeap->getCpuBase()) - surfStateSize;
    uint64_t bindlessSshBaseOffset = ptrDiff(sshHeap->getGraphicsAllocation()->getGpuAddress(), sshHeap->getGraphicsAllocation()->getGpuBaseAddress()) + offsetInHeap;
    auto patchValue = device->getNEODevice()->getGfxCoreHelper().getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(bindlessSshBaseOffset));
    auto patchLocation = reinterpret_cast<const uint32_t *>(mockKernel.getCrossThreadData());
    EXPECT_EQ(patchValue, *patchLocation);
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenRegularCmdListAppendKernelChangesHeapsAndNextKernelIsAppendedThenFinalBaseAddressStateIsDispatchedInCommandListOnce,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    checkAndPrepareBindlessKernel();

    StackVec<size_t, 3> sbaCmdsSizes = {};
    sbaCmdsSizes = {0, expectedSbaCmds, 0};
    auto sbaCmdsSizesIter = sbaCmdsSizes.begin();

    EXPECT_TRUE(commandList->stateBaseAddressTracking);

    auto &container = commandList->getCmdContainer();
    auto &cmdListStream = *container.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        cmdListStream.getUsed()));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    uint64_t ssBaseAddress = -1;
    uint64_t ssSize = -1;
    if (sshHeap) {
        ssBaseAddress = NEO::getStateBaseAddressForSsh(*sshHeap, bindlessHeapsHelper);
        ssSize = NEO::getStateSizeForSsh(*sshHeap, bindlessHeapsHelper);
    }

    uint64_t dsBaseAddress = -1;
    uint32_t dsBaseSize = 0;
    uint32_t dsFirstBaseSize = 0;

    size_t dsSize = static_cast<size_t>(-1);

    auto dshHeap = container.getIndirectHeap(NEO::HeapType::dynamicState);
    if (dshHeap) {
        dsBaseAddress = NEO::getStateBaseAddress(*dshHeap, bindlessHeapsHelper);
        dsSize = dsFirstBaseSize = static_cast<uint32_t>(NEO::getStateSize(*dshHeap, bindlessHeapsHelper));
    }
    auto statlessMocs = getMocs(true);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, requiredState.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSize, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    if (!bindlessHeapsHelper) {
        sshHeap->getSpace(sshHeap->getAvailableSpace());
        container.getHeapWithRequiredSizeAndAlignment(NEO::HeapType::surfaceState, sshHeap->getMaxAvailableSpace(), 0);
    }

    if (dshHeap) {
        dshHeap->getSpace(dshHeap->getAvailableSpace());
        container.getHeapWithRequiredSizeAndAlignment(NEO::HeapType::dynamicState, dshHeap->getMaxAvailableSpace(), 0);
    }

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (dshHeap) {
        dsBaseAddress = dshHeap->getGpuBase();
        dsBaseAddress = NEO::getStateBaseAddress(*dshHeap, bindlessHeapsHelper);
        dsBaseSize = static_cast<uint32_t>(NEO::getStateSize(*dshHeap, bindlessHeapsHelper));
    }

    if (!bindlessHeapsHelper) {
        ssBaseAddress = NEO::getStateBaseAddressForSsh(*sshHeap, bindlessHeapsHelper);
        EXPECT_NE(static_cast<int64_t>(ssBaseAddress), requiredState.surfaceStateBaseAddress.value);
    }

    if (dshHeap && !bindlessHeapsHelper) {
        EXPECT_NE(static_cast<int64_t>(dsBaseAddress), requiredState.dynamicStateBaseAddress.value);
    } else {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), requiredState.dynamicStateBaseAddress.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), finalState.surfaceStateBaseAddress.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), finalState.dynamicStateBaseAddress.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        cmdListStream.getUsed()));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds.back());

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsBaseSize, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    if (!bindlessHeapsHelper) {
        EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(sshHeap->getHeapGpuBase(), sbaCmd->getSurfaceStateBaseAddress());
    } else {
        EXPECT_FALSE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getSurfaceStateBaseAddress());
    }

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    size_t sizeBefore = cmdListStream.getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream.getCpuBase(), sizeBefore),
        cmdListStream.getUsed() - sizeBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenImmediateCmdListAppendKernelChangesHeapsAndExecuteThenFinalBaseAddressStateIsStoredInCsr,
          HeapfulSupportedMatch) {

    checkAndPrepareBindlessKernel();

    StackVec<size_t, 3> sbaCmdsSizes = {};
    sbaCmdsSizes = {0, expectedSbaCmds, expectedSbaCmds};

    auto sbaCmdsSizesIter = sbaCmdsSizes.begin();

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    NEO::StateBaseAddressPropertiesSupport sbaPropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillStateBaseAddressPropertiesSupportStructure(sbaPropertiesSupport);

    EXPECT_TRUE(commandListImmediate->stateBaseAddressTracking);

    auto &container = commandListImmediate->getCmdContainer();
    auto &cmdListImmediateStream = *container.getCommandStream();

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    auto &csrState = csrImmediate.getStreamProperties().stateBaseAddress;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t csrUsedBefore = csrStream.getUsed();
    size_t cmdListUsedBefore = 0;
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t cmdListUsedAfter = cmdListImmediateStream.getUsed();
    size_t csrUsedAfter = csrStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListImmediateStream.getCpuBase(), cmdListUsedBefore),
        cmdListUsedAfter - cmdListUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddress = NEO::getStateBaseAddressForSsh(*sshHeap, bindlessHeapsHelper);
    auto ssSize = NEO::getStateSizeForSsh(*sshHeap, bindlessHeapsHelper);

    uint64_t dsBaseAddress = -1;
    size_t dsSize = static_cast<size_t>(-1);

    auto dshHeap = container.getIndirectHeap(NEO::HeapType::dynamicState);
    if (!this->dshRequired) {
        EXPECT_EQ(nullptr, dshHeap);
    } else {
        EXPECT_NE(nullptr, dshHeap);
    }

    if (dshHeap) {
        dsBaseAddress = NEO::getStateBaseAddress(*dshHeap, bindlessHeapsHelper);
        dsSize = NEO::getStateSize(*dshHeap, bindlessHeapsHelper);
    }

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSize, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto statlessMocs = getMocs(true);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSize, csrState.dynamicStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    if (sbaPropertiesSupport.bindingTablePoolBaseAddress) {
        EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(ssSize, csrState.bindingTablePoolSize.value);
    } else {
        EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);
    }

    sshHeap->getSpace(sshHeap->getAvailableSpace());
    if (commandListImmediate->immediateCmdListHeapSharing) {
        csrImmediate.getIndirectHeap(NEO::HeapType::surfaceState, sshHeap->getMaxAvailableSpace());
    } else {
        container.getHeapWithRequiredSizeAndAlignment(NEO::HeapType::surfaceState, sshHeap->getMaxAvailableSpace(), 0);
    }

    if (dshHeap) {
        dshHeap->getSpace(dshHeap->getAvailableSpace());
        if (commandListImmediate->immediateCmdListHeapSharing) {
            csrImmediate.getIndirectHeap(NEO::HeapType::dynamicState, dshHeap->getMaxAvailableSpace());
        } else {
            container.getHeapWithRequiredSizeAndAlignment(NEO::HeapType::dynamicState, dshHeap->getMaxAvailableSpace(), 0);
        }
    }

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    csrUsedAfter = csrStream.getUsed();

    ssBaseAddress = NEO::getStateBaseAddressForSsh(*sshHeap, bindlessHeapsHelper);
    if (dshHeap) {
        dsBaseAddress = NEO::getStateBaseAddress(*dshHeap, bindlessHeapsHelper);
        dsSize = NEO::getStateSize(*dshHeap, bindlessHeapsHelper);
    }

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), csrState.dynamicStateBaseAddress.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    EXPECT_FALSE(csrState.isDirty());
    ASSERT_EQ(1u, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSize, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenRegularCmdListAppendKernelAndExecuteAndImmediateCmdListAppendKernelSharingCsrThenBaseAddressStateIsUpdatedInCsr,
          HeapfulSupportedMatch) {

    checkAndPrepareBindlessKernel();

    StackVec<size_t, 3> sbaCmdsSizes = {};
    sbaCmdsSizes = {0, expectedSbaCmds, expectedSbaCmds};

    auto sbaCmdsSizesIter = sbaCmdsSizes.begin();

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ASSERT_EQ(commandListImmediate->getCsr(false), commandQueue->getCsr());

    NEO::StateBaseAddressPropertiesSupport sbaPropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillStateBaseAddressPropertiesSupportStructure(sbaPropertiesSupport);

    EXPECT_TRUE(commandList->stateBaseAddressTracking);

    auto &container = commandList->getCmdContainer();
    auto &cmdListStream = *container.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdListStream.getCpuBase(),
        cmdListStream.getUsed()));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    uint64_t ssBaseAddress = -1;
    size_t ssSize = static_cast<size_t>(-1);
    if (!bindlessHeapsHelper) {
        ssBaseAddress = NEO::getStateBaseAddressForSsh(*sshHeap, bindlessHeapsHelper);
        ssSize = NEO::getStateSizeForSsh(*sshHeap, bindlessHeapsHelper);
    }

    uint64_t dsBaseAddress = -1;
    size_t dsSize = static_cast<size_t>(-1);
    uint32_t dsBaseSize = 0;
    auto dshHeap = container.getIndirectHeap(NEO::HeapType::dynamicState);
    if (dshHeap) {
        dsBaseAddress = NEO::getStateBaseAddress(*dshHeap, bindlessHeapsHelper);
        dsSize = dsBaseSize = static_cast<uint32_t>(NEO::getStateSize(*dshHeap, bindlessHeapsHelper));
    }

    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto statlessMocs = getMocs(true);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredState.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, requiredState.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSize, requiredState.dynamicStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredState.indirectObjectSize.value);

    if (sbaPropertiesSupport.bindingTablePoolBaseAddress) {
        EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), requiredState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(ssSize, requiredState.bindingTablePoolSize.value);
    } else {
        EXPECT_EQ(-1, requiredState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), requiredState.bindingTablePoolSize.value);
    }

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.indirectObjectBaseAddress.value, requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalState.indirectObjectSize.value, requiredState.indirectObjectSize.value);

    EXPECT_EQ(finalState.bindingTablePoolBaseAddress.value, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalState.bindingTablePoolSize.value, requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.statelessMocs.value, requiredState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    EXPECT_EQ(csrState.surfaceStateBaseAddress.value, finalState.surfaceStateBaseAddress.value);
    EXPECT_EQ(csrState.surfaceStateSize.value, finalState.surfaceStateSize.value);

    EXPECT_EQ(csrState.dynamicStateBaseAddress.value, finalState.dynamicStateBaseAddress.value);
    EXPECT_EQ(csrState.dynamicStateSize.value, finalState.dynamicStateSize.value);

    EXPECT_EQ(csrState.indirectObjectBaseAddress.value, finalState.indirectObjectBaseAddress.value);
    EXPECT_EQ(csrState.indirectObjectSize.value, finalState.indirectObjectSize.value);

    EXPECT_EQ(csrState.bindingTablePoolBaseAddress.value, finalState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(csrState.bindingTablePoolSize.value, finalState.bindingTablePoolSize.value);

    EXPECT_EQ(csrState.statelessMocs.value, finalState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsBaseSize, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    if (!bindlessHeapsHelper) {
        EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());
    } else {
        EXPECT_FALSE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getSurfaceStateBaseAddress());
    }
    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto csrIohStateCopy = csrImmediate.getIohState();
    bool iohDirty = csrIohStateCopy.updateAndCheck(container.getIndirectHeap(NEO::HeapType::indirectObject));
    EXPECT_FALSE(iohDirty);

    auto &containerImmediate = commandListImmediate->getCmdContainer();

    auto &csrStream = csrImmediate.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto sshHeapImmediate = containerImmediate.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressImmediate = NEO::getStateBaseAddressForSsh(*sshHeapImmediate, bindlessHeapsHelper);
    auto ssSizeImmediate = NEO::getStateSizeForSsh(*sshHeapImmediate, bindlessHeapsHelper);

    uint64_t dsBaseAddressImmediate = -1;
    size_t dsSizeImmediate = static_cast<size_t>(-1);

    auto dshHeapImmediate = containerImmediate.getIndirectHeap(NEO::HeapType::dynamicState);
    if (dshHeapImmediate) {
        dsBaseAddressImmediate = NEO::getStateBaseAddress(*dshHeapImmediate, bindlessHeapsHelper);
        dsSizeImmediate = NEO::getStateSize(*dshHeapImmediate, bindlessHeapsHelper);
    }

    auto ioBaseAddressImmediate = containerImmediate.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSizeImmediate = containerImmediate.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto statlessMocsImmediate = getMocs(true);

    EXPECT_EQ(static_cast<int32_t>(statlessMocsImmediate), csrState.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressImmediate), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizeImmediate, csrState.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressImmediate), csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizeImmediate, csrState.dynamicStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(ioBaseAddressImmediate), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSizeImmediate, csrState.indirectObjectSize.value);

    if (sbaPropertiesSupport.bindingTablePoolBaseAddress) {
        EXPECT_EQ(static_cast<int64_t>(ssBaseAddressImmediate), csrState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(ssSizeImmediate, csrState.bindingTablePoolSize.value);
    } else {
        EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);
    }

    cmdList.clear();

    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    if (bindlessHeapsHelper) {
        EXPECT_FALSE(csrState.isDirty());
    } else {
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
            csrUsedAfter - csrUsedBefore));
        sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
        sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

        if (this->dshRequired) {
            EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
            EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
            EXPECT_EQ(dsBaseAddressImmediate, sbaCmd->getDynamicStateBaseAddress());
            EXPECT_EQ(dsSizeImmediate, sbaCmd->getDynamicStateBufferSize());
        } else {
            EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
            EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
            EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
            EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
        }

        EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(ssBaseAddressImmediate, sbaCmd->getSurfaceStateBaseAddress());
    }
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenImmediateCmdListAppendKernelAndRegularCmdListAppendKernelAndExecuteSharingCsrThenBaseAddressStateIsUpdatedInCsr,
          HeapfulSupportedMatch) {

    checkAndPrepareBindlessKernel();

    StackVec<size_t, 2> sbaCmdsSizes = {};
    if (bindlessHeapsHelper) {
        sbaCmdsSizes = {expectedSbaCmds, 0};
    } else {
        sbaCmdsSizes = {expectedSbaCmds, expectedSbaCmds};
    }

    auto sbaCmdsSizesIter = sbaCmdsSizes.begin();

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ASSERT_EQ(commandListImmediate->getCsr(false), commandQueue->getCsr());
    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    NEO::StateBaseAddressPropertiesSupport sbaPropertiesSupport = {};
    auto &productHelper = device->getProductHelper();
    productHelper.fillStateBaseAddressPropertiesSupportStructure(sbaPropertiesSupport);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    EXPECT_TRUE(commandList->stateBaseAddressTracking);

    auto &containerImmediate = commandListImmediate->getCmdContainer();

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto sshHeapImmediate = containerImmediate.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressImmediate = NEO::getStateBaseAddressForSsh(*sshHeapImmediate, bindlessHeapsHelper);
    auto ssSizeImmediate = NEO::getStateSizeForSsh(*sshHeapImmediate, bindlessHeapsHelper);

    uint64_t dsBaseAddressImmediate = -1;
    size_t dsSizeImmediate = static_cast<size_t>(-1);

    auto dshHeapImmediate = containerImmediate.getIndirectHeap(NEO::HeapType::dynamicState);
    if (dshHeapImmediate) {
        dsBaseAddressImmediate = NEO::getStateBaseAddress(*dshHeapImmediate, bindlessHeapsHelper);
        dsSizeImmediate = NEO::getStateSize(*dshHeapImmediate, bindlessHeapsHelper);
    }

    auto ioBaseAddressImmediate = containerImmediate.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSizeImmediate = containerImmediate.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto statlessMocsImmediate = getMocs(true);

    EXPECT_EQ(static_cast<int32_t>(statlessMocsImmediate), csrState.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressImmediate), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizeImmediate, csrState.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressImmediate), csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizeImmediate, csrState.dynamicStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(ioBaseAddressImmediate), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSizeImmediate, csrState.indirectObjectSize.value);

    if (sbaPropertiesSupport.bindingTablePoolBaseAddress) {
        EXPECT_EQ(static_cast<int64_t>(ssBaseAddressImmediate), csrState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(ssSizeImmediate, csrState.bindingTablePoolSize.value);
    } else {
        EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);
    }

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressImmediate, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizeImmediate, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddressImmediate, sbaCmd->getSurfaceStateBaseAddress());

    auto &container = commandList->getCmdContainer();

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sshHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    uint64_t ssBaseAddress = -1;
    uint64_t ssSize = -1;
    if (sshHeap) {
        ssBaseAddress = NEO::getStateBaseAddressForSsh(*sshHeap, bindlessHeapsHelper);
        ssSize = NEO::getStateSizeForSsh(*sshHeap, bindlessHeapsHelper);
    }
    uint64_t dsBaseAddress = -1;
    size_t dsSize = static_cast<size_t>(-1);
    uint32_t dsBaseSize = 0;
    auto dshHeap = container.getIndirectHeap(NEO::HeapType::dynamicState);
    if (dshHeap) {
        dsBaseAddress = NEO::getStateBaseAddress(*dshHeap, bindlessHeapsHelper);
        dsSize = dsBaseSize = static_cast<uint32_t>(NEO::getStateSize(*dshHeap, bindlessHeapsHelper));
    }

    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto statlessMocs = getMocs(true);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredState.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, requiredState.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddress), requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSize, requiredState.dynamicStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredState.indirectObjectSize.value);

    if (sbaPropertiesSupport.bindingTablePoolBaseAddress) {
        EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), requiredState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(ssSize, requiredState.bindingTablePoolSize.value);
    } else {
        EXPECT_EQ(-1, requiredState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), requiredState.bindingTablePoolSize.value);
    }

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.indirectObjectBaseAddress.value, requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalState.indirectObjectSize.value, requiredState.indirectObjectSize.value);

    EXPECT_EQ(finalState.bindingTablePoolBaseAddress.value, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalState.bindingTablePoolSize.value, requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.statelessMocs.value, requiredState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    if (bindlessHeapsHelper) {
        EXPECT_EQ(-1, finalState.surfaceStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), finalState.surfaceStateSize.value);

        EXPECT_EQ(-1, finalState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), finalState.bindingTablePoolSize.value);

        EXPECT_EQ(static_cast<int64_t>(ssBaseAddressImmediate), csrState.surfaceStateBaseAddress.value);
        EXPECT_EQ(ssSizeImmediate, csrState.surfaceStateSize.value);

        EXPECT_EQ(static_cast<int64_t>(ssBaseAddressImmediate), csrState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(ssSizeImmediate, csrState.bindingTablePoolSize.value);
    } else {
        EXPECT_EQ(csrState.surfaceStateBaseAddress.value, finalState.surfaceStateBaseAddress.value);
        EXPECT_EQ(csrState.surfaceStateSize.value, finalState.surfaceStateSize.value);

        EXPECT_EQ(csrState.bindingTablePoolBaseAddress.value, finalState.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(csrState.bindingTablePoolSize.value, finalState.bindingTablePoolSize.value);
    }
    EXPECT_EQ(csrState.dynamicStateBaseAddress.value, finalState.dynamicStateBaseAddress.value);
    EXPECT_EQ(csrState.dynamicStateSize.value, finalState.dynamicStateSize.value);

    EXPECT_EQ(csrState.indirectObjectBaseAddress.value, finalState.indirectObjectBaseAddress.value);
    EXPECT_EQ(csrState.indirectObjectSize.value, finalState.indirectObjectSize.value);

    EXPECT_EQ(csrState.statelessMocs.value, finalState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    EXPECT_FALSE(finalState.isDirty());

    if (!bindlessHeapsHelper) {
        sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

        if (this->dshRequired) {
            EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
            EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
            EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());
            EXPECT_EQ(dsBaseSize, sbaCmd->getDynamicStateBufferSize());
        } else {
            EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
            EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
            EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
            EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
        }

        EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());
    }
    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenRegularCmdListAppendUncachedKernelFirstAndExecuteAndImmediateCmdListAppendUncachedKernelThenMocsStateIsUpdatedInCsr,
          HeapfulSupportedMatch) {

    checkAndPrepareBindlessKernel();

    StackVec<size_t, 3> sbaCmdsSizes = {};
    sbaCmdsSizes = {0, expectedSbaCmds, 0};

    auto sbaCmdsSizesIter = sbaCmdsSizes.begin();

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_TRUE(commandList->stateBaseAddressTracking);
    EXPECT_TRUE(commandListImmediate->stateBaseAddressTracking);

    kernel->privateState.kernelRequiresUncachedMocsCount++;

    auto &cmdStream = *commandList->getCmdContainer().getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t usedBefore = cmdStream.getUsed();
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = cmdStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream.getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto sbaList = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaList.size());

    uint32_t uncachedStatlessMocs = getMocs(false);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;
    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(uncachedStatlessMocs), requiredState.statelessMocs.value);
    EXPECT_EQ(static_cast<int32_t>(uncachedStatlessMocs), finalState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int32_t>(uncachedStatlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ((uncachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    size_t csrBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrAfter = csrStream.getUsed();

    EXPECT_EQ(static_cast<int32_t>(uncachedStatlessMocs), csrState.statelessMocs.value);

    cmdList.clear();

    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(*(sbaCmdsSizesIter++), sbaList.size());

    EXPECT_FALSE(csrState.isDirty());

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrBefore),
        csrAfter - csrBefore));
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenRegularCmdListAppendCachedKernelFirstAndExecuteAndImmediateCmdListAppendUncachedKerneThenMocsStateIsUpdatedInCsr,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_TRUE(commandList->stateBaseAddressTracking);
    EXPECT_TRUE(commandListImmediate->stateBaseAddressTracking);

    kernel->privateState.kernelRequiresUncachedMocsCount = 0;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint32_t uncachedStatlessMocs = getMocs(false);
    uint32_t cachedStatlessMocs = getMocs(true);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;
    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(cachedStatlessMocs), requiredState.statelessMocs.value);
    EXPECT_EQ(static_cast<int32_t>(cachedStatlessMocs), finalState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int32_t>(cachedStatlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ((cachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    kernel->privateState.kernelRequiresUncachedMocsCount = 1;

    size_t csrBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrAfter = csrStream.getUsed();

    EXPECT_EQ(static_cast<int32_t>(uncachedStatlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrBefore),
        csrAfter - csrBefore));

    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ((uncachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenImmediateCmdListAppendUncachedKerneAndRegularCmdListAppendCachedKernelAndExecuteThenMocsStateIsUpdatedInCsr,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_TRUE(commandList->stateBaseAddressTracking);
    EXPECT_TRUE(commandListImmediate->stateBaseAddressTracking);

    uint32_t uncachedStatlessMocs = getMocs(false);
    uint32_t cachedStatlessMocs = getMocs(true);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;
    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    kernel->privateState.kernelRequiresUncachedMocsCount = 1;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    size_t csrBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrAfter = csrStream.getUsed();

    EXPECT_EQ(static_cast<int32_t>(uncachedStatlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrBefore),
        csrAfter - csrBefore));

    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ((uncachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    kernel->privateState.kernelRequiresUncachedMocsCount = 0;

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(static_cast<int32_t>(cachedStatlessMocs), requiredState.statelessMocs.value);
    EXPECT_EQ(static_cast<int32_t>(cachedStatlessMocs), finalState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int32_t>(cachedStatlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ((cachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenStateBaseAddressTrackingWhenImmediateCmdListAppendCachedKerneAndRegularCmdListAppendUncachedKernelAndExecuteThenMocsStateIsUpdatedInCsr,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_TRUE(commandList->stateBaseAddressTracking);
    EXPECT_TRUE(commandListImmediate->stateBaseAddressTracking);

    uint32_t uncachedStatlessMocs = getMocs(false);
    uint32_t cachedStatlessMocs = getMocs(true);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;
    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    kernel->privateState.kernelRequiresUncachedMocsCount = 0;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    size_t csrBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrAfter = csrStream.getUsed();

    EXPECT_EQ(static_cast<int32_t>(cachedStatlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrBefore),
        csrAfter - csrBefore));

    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ((cachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    kernel->privateState.kernelRequiresUncachedMocsCount = 1;

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(static_cast<int32_t>(uncachedStatlessMocs), requiredState.statelessMocs.value);
    EXPECT_EQ(static_cast<int32_t>(uncachedStatlessMocs), finalState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int32_t>(uncachedStatlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_EQ((uncachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

struct HeapfulSupportedAndAtLeastXeHpCoreMatch {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsAtLeastXeCore::isMatched<productFamily>() && HeapfulSupportedMatch::isMatched<productFamily>();
    }
};

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenSbaPropertiesWhenBindingBaseAddressSetThenExpectPropertiesDataDispatched, HeapfulSupportedAndAtLeastXeHpCoreMatch) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    constexpr uint64_t bindingTablePoolBaseAddress = 0x32000;
    constexpr uint32_t bindingTablePoolSize = 0x20;
    constexpr uint64_t surfaceStateBaseAddress = 0x1200;
    constexpr uint32_t surfaceStateSize = 0x10;

    NEO::StreamProperties streamProperties;
    streamProperties.initSupport(device->getNEODevice()->getRootDeviceEnvironment());

    ze_command_queue_desc_t desc = {};
    auto cmdQueueHw = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, commandQueue->getCsr(), &desc);
    cmdQueueHw->initialize(false, false, false);

    auto &cmdQueueStream = cmdQueueHw->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    cmdQueueHw->programStateBaseAddress(0, true, cmdQueueStream, true, &streamProperties);
    size_t queueAfter = cmdQueueStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto itorBindTablePoolCmd = find<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorBindTablePoolCmd);

    streamProperties.stateBaseAddress.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);

    queueBefore = cmdQueueStream.getUsed();
    cmdQueueHw->programStateBaseAddress(0, true, cmdQueueStream, true, &streamProperties);
    queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    itorBindTablePoolCmd = find<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBindTablePoolCmd);

    auto bindTablePoolCmd = reinterpret_cast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(*itorBindTablePoolCmd);
    EXPECT_EQ(bindingTablePoolBaseAddress, bindTablePoolCmd->getBindingTablePoolBaseAddress());
    EXPECT_EQ(bindingTablePoolSize, bindTablePoolCmd->getBindingTablePoolBufferSize());

    cmdQueueHw->destroy();
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenCommandListAppendsKernelWhenCommandListIsResetThenBaseAddressPropertiesAreResetToo,
          HeapfulSupportedMatch) {

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();
    auto indirectBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto surfaceBaseAddress = container.getIndirectHeap(NEO::HeapType::surfaceState)->getHeapGpuBase();
    auto dynamicBaseAddress = static_cast<uint64_t>(NEO::StreamProperty64::initValue);
    if (dshRequired) {
        auto dsh = container.getIndirectHeap(NEO::HeapType::dynamicState);
        dynamicBaseAddress = NEO::getStateBaseAddress(*dsh, device->getNEODevice()->getBindlessHeapsHelper());
    }

    EXPECT_EQ(static_cast<int64_t>(indirectBaseAddress), commandList->currentIndirectObjectBaseAddress);
    EXPECT_EQ(static_cast<int64_t>(surfaceBaseAddress), commandList->currentSurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<int64_t>(surfaceBaseAddress), commandList->currentBindingTablePoolBaseAddress);
    EXPECT_EQ(static_cast<int64_t>(dynamicBaseAddress), commandList->currentDynamicStateBaseAddress);

    result = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::StreamProperty64::initValue, commandList->currentIndirectObjectBaseAddress);
    EXPECT_EQ(NEO::StreamProperty64::initValue, commandList->currentSurfaceStateBaseAddress);
    EXPECT_EQ(NEO::StreamProperty64::initValue, commandList->currentBindingTablePoolBaseAddress);
    EXPECT_EQ(NEO::StreamProperty64::initValue, commandList->currentDynamicStateBaseAddress);
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenCommandListAppendsKernelWhenCommandListIsResetThenStateHeapsRetainPosition,
          HeapfulSupportedMatch) {

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();
    size_t surfaceHeapUsed = container.getIndirectHeap(NEO::HeapType::surfaceState)->getUsed();
    size_t dynamicHeapUsed = 0;
    if (dshRequired) {
        dynamicHeapUsed = container.getIndirectHeap(NEO::HeapType::dynamicState)->getUsed();
    }

    result = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(surfaceHeapUsed, container.getIndirectHeap(NEO::HeapType::surfaceState)->getUsed());
    if (dshRequired) {
        EXPECT_EQ(dynamicHeapUsed, container.getIndirectHeap(NEO::HeapType::dynamicState)->getUsed());
    }
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenCommandListAppendsKernelWhenCommandListIsResetAndHeapsExhaustedBeforeFirstKernelThenStateIsReloadedInCmdQueuePreamble,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();
    auto ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);

    auto firstHeapSurfaceBaseAddress = ssh->getHeapGpuBase();

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(firstHeapSurfaceBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    ssh->getSpace(ssh->getAvailableSpace());

    result = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondHeapSurfaceBaseAddress = ssh->getHeapGpuBase();
    EXPECT_NE(firstHeapSurfaceBaseAddress, secondHeapSurfaceBaseAddress);

    queueBefore = cmdQueueStream.getUsed();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(secondHeapSurfaceBaseAddress, sbaCmd->getSurfaceStateBaseAddress());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenCommandListAppendsKernelWhenCommandListIsResetAndHeapsExhaustedBeforeSecondKernelThenStateIsReloadedInCmdList,
          HeapfulSupportedMatch) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();
    auto ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);

    auto firstHeapSurfaceBaseAddress = ssh->getHeapGpuBase();

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(firstHeapSurfaceBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    result = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdListStream = *container.getCommandStream();

    size_t usedBefore = cmdListStream.getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sizeEnoughForSingleKernel = kernel->getSurfaceStateHeapDataSize();
    ssh->getSpace(ssh->getAvailableSpace() - sizeEnoughForSingleKernel / 2);

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t usedAfter = cmdListStream.getUsed();

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondHeapSurfaceBaseAddress = ssh->getHeapGpuBase();
    EXPECT_NE(firstHeapSurfaceBaseAddress, secondHeapSurfaceBaseAddress);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream.getCpuBase(), usedBefore),
        usedAfter - usedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(secondHeapSurfaceBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    queueBefore = cmdQueueStream.getUsed();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(0u, sbaCmds.size());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenCommandListUsingPrivateSurfaceHeapWhenCommandListDestroyedThenCsrDispatchesStateCacheFlush,
          HeapfulSupportedMatch) {
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csr.commandStream;

    ze_result_t returnValue;
    L0::ult::CommandList *cmdListObject = CommandList::whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false));

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    cmdListObject->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    returnValue = cmdListObject->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdListHandle = cmdListObject->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = cmdListObject->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::findStateCacheFlushPipeControl(csr, csrStream));
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenCommandListUsingPrivateSurfaceHeapWhenOsContextNotInitializedAndCommandListDestroyedThenCsrDoNotDispatchesStateCacheFlush,
          HeapfulSupportedMatch) {
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    EngineControl &engine = neoDevice->getDefaultEngine();
    static_cast<NEO::MockOsContext *>(engine.osContext)->contextInitialized = false;
    auto &csrStream = csr.commandStream;

    ze_result_t returnValue;
    L0::ult::CommandList *cmdListObject = CommandList::whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false));

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    cmdListObject->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    returnValue = cmdListObject->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto sizeBeforeDestroy = csrStream.getUsed();

    returnValue = cmdListObject->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(sizeBeforeDestroy, csrStream.getUsed());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenCommandListUsingPrivateSurfaceHeapWhenTaskCountZeroAndCommandListDestroyedThenCsrDoNotDispatchesStateCacheFlush,
          HeapfulSupportedMatch) {

    DebugManagerStateRestore restorer;
    debugManager.flags.ContextGroupSize.set(0);
    NEO::MockDevice *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
    MockDeviceImp l0Device(mockNeoDevice);

    auto &csr = mockNeoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csr.commandStream;

    ze_result_t returnValue;
    L0::ult::CommandList *cmdListObject = CommandList::whiteboxCast(CommandList::create(productFamily, &l0Device, engineGroupType, 0u, returnValue, false));

    auto sizeBeforeDestroy = csrStream.getUsed();

    returnValue = cmdListObject->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    cmdListObject = CommandList::whiteboxCast(CommandList::create(productFamily, &l0Device, engineGroupType, 0u, returnValue, false));

    returnValue = cmdListObject->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(sizeBeforeDestroy, csrStream.getUsed());
}

HWTEST2_F(CommandListStateBaseAddressPrivateHeapTest,
          givenCommandListUsingPrivateSurfaceHeapWhenMakeCommandViewIsTrueThenDoNotReserveHeapSpaceAndDoNotConsumeCommandBuffer,
          HeapfulSupportedAndAtLeastXeHpCoreMatch) {
    checkAndPrepareBindlessKernel();

    auto &container = commandList->getCmdContainer();
    auto ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);

    GraphicsAllocation *oldGfxHeapAllocation = nullptr;
    if (ssh) {
        ssh->getSpace(ssh->getAvailableSpace() - 32);
        oldGfxHeapAllocation = ssh->getGraphicsAllocation();
    }

    uint8_t computeWalkerHostBuffer[512];
    uint8_t payloadHostBuffer[256];

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.makeKernelCommandView = true;
    launchParams.cmdWalkerBuffer = computeWalkerHostBuffer;
    launchParams.hostPayloadBuffer = payloadHostBuffer;

    auto &cmdListStream = *container.getCommandStream();
    size_t usedBefore = cmdListStream.getUsed();
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    if (ssh) {
        EXPECT_EQ(oldGfxHeapAllocation, ssh->getGraphicsAllocation());
    }

    size_t prefetchSize = 0;
    if (commandList->kernelMemoryPrefetchEnabled()) {
        prefetchSize = NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(kernel->getIndirectSize(), device->getNEODevice()->getRootDeviceEnvironment()) +
                       NEO::EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(kernel->getImmutableData()->getIsaSize(), device->getNEODevice()->getRootDeviceEnvironment());
    }

    EXPECT_EQ(usedBefore + prefetchSize, cmdListStream.getUsed());
}

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl_timestamps.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device_for_spirv.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;

HWTEST_F(CommandListCreate, givenCommandListWithInvalidWaitEventArgWhenAppendQueryKernelTimestampsThenProperErrorRetruned) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    device->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::QueryKernelTimestamps);
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

struct CmdListHelper {
    NEO::GraphicsAllocation *isaAllocation = nullptr;
    NEO::ResidencyContainer residencyContainer;
    ze_group_count_t threadGroupDimensions;
    const uint32_t *groupSize = nullptr;
    uint32_t useOnlyGlobalTimestamp = std::numeric_limits<uint32_t>::max();
};

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForAppendLaunchKernel : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {

  public:
    CmdListHelper cmdListHelper;
    ze_result_t appendLaunchKernel(ze_kernel_handle_t hKernel,
                                   const ze_group_count_t *pThreadGroupDimensions,
                                   ze_event_handle_t hEvent,
                                   uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents) override {

        const auto kernel = Kernel::fromHandle(hKernel);
        cmdListHelper.isaAllocation = kernel->getIsaAllocation();
        cmdListHelper.residencyContainer = kernel->getResidencyContainer();
        cmdListHelper.groupSize = kernel->getGroupSize();
        cmdListHelper.threadGroupDimensions = *pThreadGroupDimensions;

        auto kernelName = kernel->getImmutableData()->getDescriptor().kernelMetadata.kernelName;
        NEO::ArgDescriptor arg;
        if (kernelName == "QueryKernelTimestamps") {
            arg = kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[2u];
        } else if (kernelName == "QueryKernelTimestampsWithOffsets") {
            arg = kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[3u];
        } else {
            return ZE_RESULT_SUCCESS;
        }
        auto crossThreadData = kernel->getCrossThreadData();
        auto element = arg.as<NEO::ArgDescValue>().elements[0];
        auto pDst = ptrOffset(crossThreadData, element.offset);
        cmdListHelper.useOnlyGlobalTimestamp = *(uint32_t *)(pDst);

        return ZE_RESULT_SUCCESS;
    }
};

using AppendQueryKernelTimestamps = CommandListCreate;

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsWithoutOffsetsThenProperBuiltinWasAdded, IsAtLeastSkl) {
    std::unique_ptr<MockDeviceForSpv<false, false>> testDevice = std::unique_ptr<MockDeviceForSpv<false, false>>(new MockDeviceForSpv<false, false>(device->getNEODevice(), device->getNEODevice()->getExecutionEnvironment(), driverHandle.get()));
    testDevice->builtins.reset(new MockBuiltinFunctionsLibImplTimestamps(testDevice.get(), testDevice->getNEODevice()->getBuiltIns()));
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::QueryKernelTimestamps);
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::QueryKernelTimestampsWithOffsets);

    device = testDevice.get();

    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(device->toHandle(), device));
    auto result = context->allocDeviceMem(device, &deviceDesc, 128, 1, &alloc);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    bool containsDstPtr = false;
    bool gpuTimeStampAlloc = false;
    for (auto &residentGfxAlloc : commandList.cmdListHelper.residencyContainer) {
        if (residentGfxAlloc != nullptr) {
            if (residentGfxAlloc->getGpuAddress() ==
                reinterpret_cast<uint64_t>(alloc)) {
                containsDstPtr = true;
            }
            if (residentGfxAlloc->getAllocationType() ==
                NEO::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER) {
                gpuTimeStampAlloc = true;
            }
        }
    }

    EXPECT_TRUE(containsDstPtr);
    EXPECT_TRUE(gpuTimeStampAlloc);

    EXPECT_EQ(testDevice->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestamps)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());
    EXPECT_EQ(2u, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[2]);

    EXPECT_EQ(NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    context->freeMem(alloc);
}

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsWithOffsetsThenProperBuiltinWasAdded, IsAtLeastSkl) {
    std::unique_ptr<MockDeviceForSpv<false, false>> testDevice = std::unique_ptr<MockDeviceForSpv<false, false>>(new MockDeviceForSpv<false, false>(device->getNEODevice(), device->getNEODevice()->getExecutionEnvironment(), driverHandle.get()));
    testDevice->builtins.reset(new MockBuiltinFunctionsLibImplTimestamps(testDevice.get(), testDevice->getNEODevice()->getBuiltIns()));
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::QueryKernelTimestamps);
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::QueryKernelTimestampsWithOffsets);

    device = testDevice.get();

    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(device->toHandle(), device));
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

    for (auto &a : commandList.cmdListHelper.residencyContainer) {
        if (a != nullptr && a->getGpuAddress() == reinterpret_cast<uint64_t>(alloc)) {
            containsDstPtr = true;
        }
    }

    EXPECT_TRUE(containsDstPtr);

    bool containOffsetPtr = false;

    for (auto &a : commandList.cmdListHelper.residencyContainer) {
        if (a != nullptr && a->getGpuAddress() == reinterpret_cast<uint64_t>(offsetAlloc)) {
            containOffsetPtr = true;
        }
    }

    EXPECT_TRUE(containOffsetPtr);

    EXPECT_EQ(device->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestampsWithOffsets)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());
    EXPECT_EQ(2u, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(1u, commandList.cmdListHelper.groupSize[2]);

    EXPECT_EQ(NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    context->freeMem(alloc);
    context->freeMem(offsetAlloc);
}

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsWithEventsNumberBiggerThanMaxWorkItemSizeThenProperGroupSizeAndGroupCountIsSet, IsAtLeastSkl) {
    std::unique_ptr<MockDeviceForSpv<false, false>> testDevice = std::unique_ptr<MockDeviceForSpv<false, false>>(new MockDeviceForSpv<false, false>(device->getNEODevice(), device->getNEODevice()->getExecutionEnvironment(), driverHandle.get()));
    testDevice->builtins.reset(new MockBuiltinFunctionsLibImplTimestamps(testDevice.get(), testDevice->getNEODevice()->getBuiltIns()));
    testDevice->getBuiltinFunctionsLib()->initBuiltinKernel(L0::Builtin::QueryKernelTimestamps);

    device = testDevice.get();

    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(device->toHandle(), device));
    auto result = context->allocDeviceMem(device, &deviceDesc, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    size_t eventCount = device->getNEODevice()->getDeviceInfo().maxWorkItemSizes[0] * 2u;
    std::unique_ptr<ze_event_handle_t[]> events = std::make_unique<ze_event_handle_t[]>(eventCount);

    for (size_t i = 0u; i < eventCount; ++i) {
        events[i] = event.toHandle();
    }

    result = commandList.appendQueryKernelTimestamps(static_cast<uint32_t>(eventCount), events.get(), alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(device->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestamps)->getIsaAllocation()->getGpuAddress(), commandList.cmdListHelper.isaAllocation->getGpuAddress());

    uint32_t groupSizeX = static_cast<uint32_t>(eventCount);
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    device->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestamps)->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);

    EXPECT_EQ(groupSizeX, commandList.cmdListHelper.groupSize[0]);
    EXPECT_EQ(groupSizeY, commandList.cmdListHelper.groupSize[1]);
    EXPECT_EQ(groupSizeZ, commandList.cmdListHelper.groupSize[2]);

    EXPECT_EQ(NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps() ? 1u : 0u, commandList.cmdListHelper.useOnlyGlobalTimestamp);

    EXPECT_EQ(static_cast<uint32_t>(eventCount) / groupSizeX, commandList.cmdListHelper.threadGroupDimensions.groupCountX);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountY);
    EXPECT_EQ(1u, commandList.cmdListHelper.threadGroupDimensions.groupCountZ);

    context->freeMem(alloc);
}

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsAndInvalidResultSuggestGroupSizeThenUnknownResultReturned, IsAtLeastSkl) {
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
            execEnvironment = device->getExecEnvironment();
            driverHandle = device->getDriverHandle();
            tmpMockBultinLib = new MockBuiltinFunctionsForQueryKernelTimestamps{nullptr, nullptr};
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

    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;

    commandList.initialize(&mockDevice, NEO::EngineGroupType::RenderCompute, 0u);

    MockEvent event;
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(mockDevice.toHandle(), &mockDevice));
    auto result = context->allocDeviceMem(&mockDevice, &deviceDesc, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    context->freeMem(alloc);
}

HWTEST2_F(AppendQueryKernelTimestamps, givenCommandListWhenAppendQueryKernelTimestampsAndInvalidResultSetGroupSizeThenUnknownResultReturned, IsAtLeastSkl) {
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
            execEnvironment = device->getExecEnvironment();
            driverHandle = device->getDriverHandle();
            tmpMockBultinLib = new MockBuiltinFunctionsForQueryKernelTimestamps{nullptr, nullptr};
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

    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;

    commandList.initialize(&mockDevice, NEO::EngineGroupType::RenderCompute, 0u);

    MockEvent event;
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(mockDevice.toHandle(), &mockDevice));
    auto result = context->allocDeviceMem(&mockDevice, &deviceDesc, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList.appendQueryKernelTimestamps(2u, events, alloc, nullptr, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

    context->freeMem(alloc);
}

HWTEST2_F(AppendQueryKernelTimestamps, givenEventWhenAppendQueryIsCalledThenSetAllEventData, IsAtLeastSkl) {
    class MockQueryKernelTimestampsKernel : public L0::KernelImp {
      public:
        MockQueryKernelTimestampsKernel(L0::Module *module) : KernelImp(module) {
            mockKernelImmutableData.kernelDescriptor = &mockKernelDescriptor;
            this->kernelImmData = &mockKernelImmutableData;
        }

        ze_result_t setArgBufferWithAlloc(uint32_t argIndex, uintptr_t argVal, NEO::GraphicsAllocation *allocation) override {
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

        NEO::GraphicsAllocation *index0Allocation = nullptr;
        KernelDescriptor mockKernelDescriptor = {};
        WhiteBox<::L0::KernelImmutableData> mockKernelImmutableData = {};
    };

    struct MockBuiltinFunctionsForQueryKernelTimestamps : BuiltinFunctionsLibImpl {
        MockBuiltinFunctionsForQueryKernelTimestamps(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {
            tmpModule = std::make_unique<MockModule>(device, nullptr, ModuleType::Builtin);
            tmpMockKernel = std::make_unique<MockQueryKernelTimestampsKernel>(static_cast<L0::ModuleImp *>(tmpModule.get()));
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
            execEnvironment = device->getExecEnvironment();
            driverHandle = device->getDriverHandle();
            tmpMockBultinLib = std::make_unique<MockBuiltinFunctionsForQueryKernelTimestamps>(this, nullptr);
        }
        MockBuiltinFunctionsForQueryKernelTimestamps *getBuiltinFunctionsLib() override {
            return tmpMockBultinLib.get();
        }
        std::unique_ptr<MockBuiltinFunctionsForQueryKernelTimestamps> tmpMockBultinLib;
    };

    MockDeviceHandle mockDevice;
    mockDevice.initialize(device);

    MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;

    commandList.initialize(&mockDevice, NEO::EngineGroupType::RenderCompute, 0u);

    MockEvent event;
    ze_event_handle_t events[2] = {event.toHandle(), event.toHandle()};
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->getDevices().insert(std::make_pair(mockDevice.toHandle(), &mockDevice));
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    commandList->appendSignalEvent(event.toHandle());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWhenAppendSignalEventWithScopeThenPipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    commandList->appendSignalEvent(event.toHandle());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenAppendWaitEventsWithDcFlushThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    auto eventHandle = event.toHandle();
    commandList->appendWaitOnEvents(1, &eventHandle);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    if (MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo)) {
        EXPECT_NE(cmdList.end(), itor);
    } else {
        EXPECT_EQ(cmdList.end(), itor);
    }
}

HWTEST_F(CommandListCreate, givenCommandListyWhenAppendWaitEventsWithDcFlushThenPipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    auto eventHandle = event.toHandle();
    commandList->appendWaitOnEvents(1, &eventHandle);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWhenAppendWaitEventsWithDcFlushThenPipeControlIsProgrammedOnlyOnce) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event2.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    ze_event_handle_t events[] = {&event, &event2};

    commandList->appendWaitOnEvents(2, events);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    auto itor2 = find<SEMAPHORE_WAIT *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor2);
}

HWTEST_F(CommandListCreate, givenFlushTaskFlagEnabledAndAsyncCmdQueueAndCopyOnlyImmediateCommandListWhenAppendWaitEventsWithHostScopeThenMiFlushAndSemWaitAreAdded) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    auto &commandContainer = commandList->commandContainer;
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = ZE_EVENT_SCOPE_FLAG_HOST;
    event2.waitScope = 0;
    ze_event_handle_t events[] = {&event, &event2};

    auto used = commandContainer.getCommandStream()->getUsed();
    commandList->appendWaitOnEvents(2, events);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
    EXPECT_EQ(used, commandContainer.getCommandStream()->getUsed());
}

HWTEST_F(CommandListCreate, givenAsyncCmdQueueAndCopyOnlyImmediateCommandListWhenAppendWaitEventsWithSubdeviceScopeThenMiFlushAndSemWaitAreAddedViaFlushTask) {
    using SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    auto &commandContainer = commandList->commandContainer;
    MockEvent event, event2;
    event.signalScope = 0;
    event.waitScope = 0;
    event2.waitScope = 0;
    ze_event_handle_t events[] = {&event, &event2};

    auto used = commandContainer.getCommandStream()->getUsed();
    commandList->appendWaitOnEvents(2, events);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
    EXPECT_EQ(used, commandContainer.getCommandStream()->getUsed());
}

HWTEST_F(CommandListCreate, givenFlushTaskFlagEnabledAndAsyncCmdQueueWithCopyOnlyImmediateCommandListCreatedThenSlushTaskSubmissionIsSetToFalse) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::Copy, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(false, commandList->isFlushTaskSubmissionEnabled);
}

HWTEST2_F(CommandListCreate, givenIndirectAccessFlagsAreChangedWhenResetingCommandListThenExpectAllFlagsSetToDefault, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_FALSE(commandList->indirectAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectSharedAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectDeviceAllocationsAllowed);

    commandList->indirectAllocationsAllowed = true;
    commandList->unifiedMemoryControls.indirectHostAllocationsAllowed = true;
    commandList->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
    commandList->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;

    returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_FALSE(commandList->indirectAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectSharedAllocationsAllowed);
    EXPECT_FALSE(commandList->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
}

HWTEST2_F(CommandListCreate, whenContainsCooperativeKernelsIsCalledThenCorrectValueIsReturned, IsAtLeastSkl) {
    for (auto testValue : ::testing::Bool()) {
        MockCommandListForAppendLaunchKernel<gfxCoreFamily> commandList;
        commandList.initialize(device, NEO::EngineGroupType::Compute, 0u);
        commandList.containsCooperativeKernelsFlag = testValue;
        EXPECT_EQ(testValue, commandList.containsCooperativeKernels());
        commandList.reset();
        EXPECT_FALSE(commandList.containsCooperativeKernels());
    }
}

HWTEST_F(CommandListCreate, GivenSingleTileDeviceWhenCommandListIsResetThenPartitionCountIsReversedToOne) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::Compute,
                                                                     0u,
                                                                     returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);

    returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

HWTEST_F(CommandListCreate, WhenReservingSpaceThenCommandsAddedToBatchBuffer) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    ASSERT_NE(nullptr, commandList->commandContainer.getCommandStream());
    auto commandStream = commandList->commandContainer.getCommandStream();

    auto usedSpaceBefore = commandStream->getUsed();

    using MI_NOOP = typename FamilyType::MI_NOOP;
    MI_NOOP cmd = FamilyType::cmdInitNoop;
    uint32_t uniqueIDforTest = 0x12345u;
    cmd.setIdentificationNumber(uniqueIDforTest);

    size_t sizeToReserveForCommand = sizeof(cmd);
    void *ptrToReservedMemory = nullptr;
    returnValue = commandList->reserveSpace(sizeToReserveForCommand, &ptrToReservedMemory);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    if (ptrToReservedMemory != nullptr) {
        *reinterpret_cast<MI_NOOP *>(ptrToReservedMemory) = cmd;
    }

    auto usedSpaceAfter = commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandStream->getCpuBase(), usedSpaceAfter));

    auto itor = cmdList.begin();
    while (itor != cmdList.end()) {
        using MI_NOOP = typename FamilyType::MI_NOOP;
        itor = find<MI_NOOP *>(itor, cmdList.end());
        if (itor == cmdList.end())
            break;

        auto cmd = genCmdCast<MI_NOOP *>(*itor);
        if (uniqueIDforTest == cmd->getIdentificationNumber()) {
            break;
        }

        itor++;
    }
    ASSERT_NE(itor, cmdList.end());
}

TEST_F(CommandListCreate, givenOrdinalBiggerThanAvailableEnginesWhenCreatingCommandListThenInvalidArgumentErrorIsReturned) {
    auto numAvailableEngineGroups = static_cast<uint32_t>(neoDevice->getRegularEngineGroups().size());
    ze_command_list_handle_t commandList = nullptr;
    ze_command_list_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    desc.commandQueueGroupOrdinal = numAvailableEngineGroups;
    auto returnValue = device->createCommandList(&desc, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);

    ze_command_queue_desc_t desc2 = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc2.ordinal = numAvailableEngineGroups;
    desc2.index = 0;
    returnValue = device->createCommandListImmediate(&desc2, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);

    desc2.ordinal = 0;
    desc2.index = 0x1000;
    returnValue = device->createCommandListImmediate(&desc2, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);
}

TEST_F(CommandListCreate, givenRootDeviceAndImplicitScalingDisabledWhenCreatingCommandListThenValidateQueueOrdinalUsingSubDeviceEngines) {
    NEO::UltDeviceFactory deviceFactory{1, 2};
    auto &rootDevice = *deviceFactory.rootDevices[0];
    auto &subDevice0 = *deviceFactory.subDevices[0];
    rootDevice.regularEngineGroups.resize(1);
    subDevice0.getRegularEngineGroups().push_back(NEO::Device::EngineGroupT{});
    subDevice0.getRegularEngineGroups().back().engineGroupType = EngineGroupType::Compute;
    subDevice0.getRegularEngineGroups().back().engines.resize(1);
    subDevice0.getRegularEngineGroups().back().engines[0].commandStreamReceiver = &rootDevice.getGpgpuCommandStreamReceiver();
    auto ordinal = static_cast<uint32_t>(subDevice0.getRegularEngineGroups().size() - 1);
    Mock<L0::DeviceImp> l0RootDevice(&rootDevice, rootDevice.getExecutionEnvironment());

    ze_command_list_handle_t commandList = nullptr;
    ze_command_list_desc_t cmdDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdDesc.commandQueueGroupOrdinal = ordinal;
    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.ordinal = ordinal;
    queueDesc.index = 0;

    l0RootDevice.driverHandle = driverHandle.get();

    l0RootDevice.implicitScalingCapable = true;
    auto returnValue = l0RootDevice.createCommandList(&cmdDesc, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);

    returnValue = l0RootDevice.createCommandListImmediate(&queueDesc, &commandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
    EXPECT_EQ(nullptr, commandList);

    l0RootDevice.implicitScalingCapable = false;
    returnValue = l0RootDevice.createCommandList(&cmdDesc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_NE(nullptr, commandList);
    L0::CommandList::fromHandle(commandList)->destroy();
    commandList = nullptr;

    returnValue = l0RootDevice.createCommandListImmediate(&queueDesc, &commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_NE(nullptr, commandList);
    L0::CommandList::fromHandle(commandList)->destroy();
}

} // namespace ult
} // namespace L0

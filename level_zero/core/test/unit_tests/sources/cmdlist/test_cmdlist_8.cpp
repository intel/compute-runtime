/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/event/event_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace NEO {
namespace SysCalls {
extern bool getNumThreadsCalled;
}
} // namespace NEO

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;
using CommandListAppendLaunchKernel = Test<ModuleFixture>;

struct AppendMemoryLockedCopyFixture : public DeviceFixture {
    void setUp() {
        debugManager.flags.ExperimentalCopyThroughLock.set(1);
        debugManager.flags.EnableLocalMemory.set(1);
        DeviceFixture::setUp();

        nonUsmHostPtr = new char[sz];
        ze_host_mem_alloc_desc_t hostDesc = {};
        context->allocHostMem(&hostDesc, sz, 1u, &hostPtr);

        ze_device_mem_alloc_desc_t deviceDesc = {};
        context->allocDeviceMem(device->toHandle(), &deviceDesc, sz, 1u, &devicePtr);

        context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, sz, 1u, &sharedPtr);
    }
    void tearDown() {
        delete[] nonUsmHostPtr;
        context->freeMem(hostPtr);
        context->freeMem(devicePtr);
        context->freeMem(sharedPtr);
        DeviceFixture::tearDown();
    }

    DebugManagerStateRestore restore;
    CmdListMemoryCopyParams copyParams = {};
    char *nonUsmHostPtr;
    void *hostPtr;
    void *devicePtr;
    void *sharedPtr;
    size_t sz = 4 * MemoryConstants::megaByte;
};

using AppendMemoryLockedCopyTest = Test<AppendMemoryLockedCopyFixture>;

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndImportedHostPtrAsOperandThenItIsTreatedAsHostUsmPtr) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    auto importedPtr = new char[sz]();
    EXPECT_NE(nullptr, importedPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getDriverHandle()->importExternalPointer(importedPtr, sz));

    std::array<size_t, 4> sizes = {1, 256, 4096, sz};
    for (size_t i = 0; i < sizes.size(); i++) {
        CpuMemCopyInfo copyInfoDeviceUsmToHostUsm(hostPtr, devicePtr, sizes[i]);
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(devicePtr, sizes[i], copyInfoDeviceUsmToHostUsm.srcAllocData));
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(hostPtr, sizes[i], copyInfoDeviceUsmToHostUsm.dstAllocData));
        EXPECT_NE(nullptr, copyInfoDeviceUsmToHostUsm.srcAllocData);
        EXPECT_NE(nullptr, copyInfoDeviceUsmToHostUsm.dstAllocData);

        CpuMemCopyInfo copyInfoDeviceUsmToHostImported(importedPtr, devicePtr, sizes[i]);
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(devicePtr, sizes[i], copyInfoDeviceUsmToHostImported.srcAllocData));
        EXPECT_FALSE(device->getDriverHandle()->findAllocationDataForRange(importedPtr, sizes[i], copyInfoDeviceUsmToHostImported.dstAllocData));
        EXPECT_NE(nullptr, copyInfoDeviceUsmToHostImported.srcAllocData);
        EXPECT_EQ(nullptr, copyInfoDeviceUsmToHostImported.dstAllocData);

        EXPECT_EQ(cmdList.preferCopyThroughLockedPtr(copyInfoDeviceUsmToHostUsm, 0, nullptr), cmdList.preferCopyThroughLockedPtr(copyInfoDeviceUsmToHostImported, 0, nullptr));
        EXPECT_TRUE(copyInfoDeviceUsmToHostImported.dstIsImportedHostPtr);

        CpuMemCopyInfo copyInfoHostUsmToDeviceUsm(devicePtr, hostPtr, sizes[i]);
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(hostPtr, sizes[i], copyInfoHostUsmToDeviceUsm.srcAllocData));
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(devicePtr, sizes[i], copyInfoHostUsmToDeviceUsm.dstAllocData));
        EXPECT_NE(nullptr, copyInfoHostUsmToDeviceUsm.srcAllocData);
        EXPECT_NE(nullptr, copyInfoHostUsmToDeviceUsm.dstAllocData);

        CpuMemCopyInfo copyInfoHostImportedToDeviceUsm(devicePtr, importedPtr, sizes[i]);
        EXPECT_FALSE(device->getDriverHandle()->findAllocationDataForRange(importedPtr, sizes[i], copyInfoHostImportedToDeviceUsm.srcAllocData));
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(devicePtr, sizes[i], copyInfoHostImportedToDeviceUsm.dstAllocData));
        EXPECT_EQ(nullptr, copyInfoHostImportedToDeviceUsm.srcAllocData);
        EXPECT_NE(nullptr, copyInfoHostImportedToDeviceUsm.dstAllocData);

        EXPECT_EQ(cmdList.preferCopyThroughLockedPtr(copyInfoHostUsmToDeviceUsm, 0, nullptr), cmdList.preferCopyThroughLockedPtr(copyInfoHostImportedToDeviceUsm, 0, nullptr));
        EXPECT_TRUE(copyInfoHostImportedToDeviceUsm.srcIsImportedHostPtr);

        CpuMemCopyInfo copyInfoSharedUsmToHostUsm(hostPtr, sharedPtr, sizes[i]);
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(sharedPtr, sizes[i], copyInfoSharedUsmToHostUsm.srcAllocData));
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(hostPtr, sizes[i], copyInfoSharedUsmToHostUsm.dstAllocData));
        EXPECT_NE(nullptr, copyInfoSharedUsmToHostUsm.srcAllocData);
        EXPECT_NE(nullptr, copyInfoSharedUsmToHostUsm.dstAllocData);

        CpuMemCopyInfo copyInfoSharedUsmToHostImported(importedPtr, sharedPtr, sizes[i]);
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(sharedPtr, sizes[i], copyInfoSharedUsmToHostImported.srcAllocData));
        EXPECT_FALSE(device->getDriverHandle()->findAllocationDataForRange(importedPtr, sizes[i], copyInfoSharedUsmToHostImported.dstAllocData));
        EXPECT_NE(nullptr, copyInfoSharedUsmToHostImported.srcAllocData);
        EXPECT_EQ(nullptr, copyInfoSharedUsmToHostImported.dstAllocData);

        EXPECT_EQ(cmdList.preferCopyThroughLockedPtr(copyInfoSharedUsmToHostUsm, 0, nullptr), cmdList.preferCopyThroughLockedPtr(copyInfoSharedUsmToHostImported, 0, nullptr));
        EXPECT_TRUE(copyInfoSharedUsmToHostImported.dstIsImportedHostPtr);

        CpuMemCopyInfo copyInfoHostUsmToSharedUsm(sharedPtr, hostPtr, sizes[i]);
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(hostPtr, sizes[i], copyInfoHostUsmToSharedUsm.srcAllocData));
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(sharedPtr, sizes[i], copyInfoHostUsmToSharedUsm.dstAllocData));
        EXPECT_NE(nullptr, copyInfoHostUsmToSharedUsm.srcAllocData);
        EXPECT_NE(nullptr, copyInfoHostUsmToSharedUsm.dstAllocData);

        CpuMemCopyInfo copyInfoHostImportedToSharedUsm(sharedPtr, importedPtr, sizes[i]);
        EXPECT_FALSE(device->getDriverHandle()->findAllocationDataForRange(importedPtr, sizes[i], copyInfoHostImportedToSharedUsm.srcAllocData));
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(sharedPtr, sizes[i], copyInfoHostImportedToSharedUsm.dstAllocData));
        EXPECT_EQ(nullptr, copyInfoHostImportedToSharedUsm.srcAllocData);
        EXPECT_NE(nullptr, copyInfoHostImportedToSharedUsm.dstAllocData);

        EXPECT_EQ(cmdList.preferCopyThroughLockedPtr(copyInfoHostUsmToSharedUsm, 0, nullptr), cmdList.preferCopyThroughLockedPtr(copyInfoHostImportedToSharedUsm, 0, nullptr));
        EXPECT_TRUE(copyInfoHostImportedToSharedUsm.srcIsImportedHostPtr);

        CpuMemCopyInfo copyInfoHostUsmToHostUsm(hostPtr, hostPtr, sizes[i]);
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(hostPtr, sizes[i], copyInfoHostUsmToHostUsm.srcAllocData));
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(hostPtr, sizes[i], copyInfoHostUsmToHostUsm.dstAllocData));
        EXPECT_NE(nullptr, copyInfoHostUsmToHostUsm.srcAllocData);
        EXPECT_NE(nullptr, copyInfoHostUsmToHostUsm.dstAllocData);

        CpuMemCopyInfo copyInfoHostUsmToHostImported(importedPtr, hostPtr, sizes[i]);
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(hostPtr, sizes[i], copyInfoHostUsmToHostImported.srcAllocData));
        EXPECT_FALSE(device->getDriverHandle()->findAllocationDataForRange(importedPtr, sizes[i], copyInfoHostUsmToHostImported.dstAllocData));
        EXPECT_NE(nullptr, copyInfoHostUsmToHostImported.srcAllocData);
        EXPECT_EQ(nullptr, copyInfoHostUsmToHostImported.dstAllocData);

        EXPECT_EQ(cmdList.preferCopyThroughLockedPtr(copyInfoHostUsmToHostUsm, 0, nullptr), cmdList.preferCopyThroughLockedPtr(copyInfoHostUsmToHostImported, 0, nullptr));
        EXPECT_TRUE(copyInfoHostUsmToHostImported.dstIsImportedHostPtr);

        CpuMemCopyInfo copyInfoHostImportedToHostUsm(hostPtr, importedPtr, sizes[i]);
        EXPECT_FALSE(device->getDriverHandle()->findAllocationDataForRange(importedPtr, sizes[i], copyInfoHostImportedToHostUsm.srcAllocData));
        EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(hostPtr, sizes[i], copyInfoHostImportedToHostUsm.dstAllocData));
        EXPECT_EQ(nullptr, copyInfoHostImportedToHostUsm.srcAllocData);
        EXPECT_NE(nullptr, copyInfoHostImportedToHostUsm.dstAllocData);

        EXPECT_EQ(cmdList.preferCopyThroughLockedPtr(copyInfoHostUsmToHostUsm, 0, nullptr), cmdList.preferCopyThroughLockedPtr(copyInfoHostImportedToHostUsm, 0, nullptr));
        EXPECT_TRUE(copyInfoHostImportedToHostUsm.srcIsImportedHostPtr);

        CpuMemCopyInfo copyInfoHostImportedToHostImported(importedPtr, importedPtr, sizes[i]);
        EXPECT_FALSE(device->getDriverHandle()->findAllocationDataForRange(importedPtr, sizes[i], copyInfoHostImportedToHostImported.srcAllocData));
        EXPECT_FALSE(device->getDriverHandle()->findAllocationDataForRange(importedPtr, sizes[i], copyInfoHostImportedToHostImported.dstAllocData));
        EXPECT_EQ(nullptr, copyInfoHostImportedToHostImported.srcAllocData);
        EXPECT_EQ(nullptr, copyInfoHostImportedToHostImported.dstAllocData);

        EXPECT_EQ(cmdList.preferCopyThroughLockedPtr(copyInfoHostUsmToHostUsm, 0, nullptr), cmdList.preferCopyThroughLockedPtr(copyInfoHostImportedToHostImported, 0, nullptr));
        EXPECT_TRUE(copyInfoHostImportedToHostImported.srcIsImportedHostPtr);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getDriverHandle()->releaseImportedPointer(importedPtr));
    delete[] importedPtr;
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenPreferCopyThroughLockedPtrCalledForH2DThenReturnTrue) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    CpuMemCopyInfo cpuMemCopyInfo(devicePtr, nonUsmHostPtr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(nonUsmHostPtr, 1024, cpuMemCopyInfo.srcAllocData);
    ASSERT_FALSE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, cpuMemCopyInfo.dstAllocData);
    ASSERT_TRUE(dstFound);
    EXPECT_TRUE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 0, nullptr));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenPreferCopyThroughLockedPtrCalledForD2HThenReturnTrue) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    CpuMemCopyInfo cpuMemCopyInfo(nonUsmHostPtr, devicePtr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, cpuMemCopyInfo.srcAllocData);
    ASSERT_TRUE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(nonUsmHostPtr, 1024, cpuMemCopyInfo.dstAllocData);
    ASSERT_FALSE(dstFound);
    EXPECT_TRUE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 0, nullptr));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndUsmHostPtrWhenPreferCopyThroughLockedPtrCalledForH2DThenReturnTrue) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    CpuMemCopyInfo cpuMemCopyInfo(devicePtr, hostPtr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(hostPtr, 1024, cpuMemCopyInfo.srcAllocData);
    ASSERT_TRUE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, cpuMemCopyInfo.dstAllocData);
    ASSERT_TRUE(dstFound);
    EXPECT_TRUE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 0, nullptr));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndUsmHostPtrWhenPreferCopyThroughLockedPtrCalledForH2DWhenCopyCantBePerformedImmediatelyThenReturnFalse) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    CpuMemCopyInfo cpuMemCopyInfo(devicePtr, hostPtr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(hostPtr, 1024, cpuMemCopyInfo.srcAllocData);
    ASSERT_TRUE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, cpuMemCopyInfo.dstAllocData);
    ASSERT_TRUE(dstFound);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;
    ze_result_t returnValue;
    std::unique_ptr<L0::EventPool> eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(device->getDriverHandle(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_event_handle_t event = nullptr;
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));
    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));

    cmdList.dependenciesPresent = false;
    EXPECT_FALSE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 1, &event));

    cmdList.dependenciesPresent = true;
    EXPECT_FALSE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 0, nullptr));

    cmdList.dependenciesPresent = true;
    EXPECT_FALSE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 1, &event));

    eventObject->setIsCompleted();
    cmdList.dependenciesPresent = false;
    EXPECT_TRUE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 1, &event));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndUsmHostPtrWhenPreferCopyThroughLockedPtrCalledForD2HWhenCopyCantBePerformedImmediatelyThenReturnFalse) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    CpuMemCopyInfo cpuMemCopyInfo(hostPtr, devicePtr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(hostPtr, 1024, cpuMemCopyInfo.srcAllocData);
    ASSERT_TRUE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, cpuMemCopyInfo.dstAllocData);
    ASSERT_TRUE(dstFound);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;
    ze_result_t returnValue;
    std::unique_ptr<L0::EventPool> eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(device->getDriverHandle(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_event_handle_t event = nullptr;
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));
    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));

    cmdList.dependenciesPresent = false;
    EXPECT_FALSE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 1, &event));

    cmdList.dependenciesPresent = true;
    EXPECT_FALSE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 0, nullptr));

    cmdList.dependenciesPresent = true;
    EXPECT_FALSE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 1, &event));

    eventObject->setIsCompleted();
    cmdList.dependenciesPresent = false;
    EXPECT_TRUE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 1, &event));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndIpcDevicePtrWhenPreferCopyThroughLockedPtrCalledForD2HThenReturnFalse) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_mem_handle_t ipcHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->getIpcMemHandle(devicePtr, &ipcHandle));
    ze_ipc_memory_flag_t ipcFlags{};
    void *ipcDevicePtr = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->openIpcMemHandle(device->toHandle(), ipcHandle, ipcFlags, &ipcDevicePtr));
    EXPECT_NE(nullptr, ipcDevicePtr);

    CpuMemCopyInfo cpuMemCopyInfo(hostPtr, ipcDevicePtr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(ipcDevicePtr, 1024, cpuMemCopyInfo.srcAllocData);
    ASSERT_TRUE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(hostPtr, 1024, cpuMemCopyInfo.dstAllocData);
    ASSERT_TRUE(dstFound);
    EXPECT_FALSE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->closeIpcMemHandle(ipcDevicePtr));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndIpcDevicePtrWhenPreferCopyThroughLockedPtrCalledForH2DThenReturnFalse) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_mem_handle_t ipcHandle{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->getIpcMemHandle(devicePtr, &ipcHandle));
    ze_ipc_memory_flag_t ipcFlags{};
    void *ipcDevicePtr = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, context->openIpcMemHandle(device->toHandle(), ipcHandle, ipcFlags, &ipcDevicePtr));
    EXPECT_NE(nullptr, ipcDevicePtr);

    CpuMemCopyInfo cpuMemCopyInfo(ipcDevicePtr, hostPtr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(hostPtr, 1024, cpuMemCopyInfo.srcAllocData);
    ASSERT_TRUE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(ipcDevicePtr, 1024, cpuMemCopyInfo.dstAllocData);
    ASSERT_TRUE(dstFound);
    EXPECT_FALSE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 0, nullptr));

    EXPECT_EQ(ZE_RESULT_SUCCESS, context->closeIpcMemHandle(ipcDevicePtr));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenIsSuitableUSMDeviceAllocThenReturnCorrectValue) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    CpuMemCopyInfo cpuMemCopyInfo(devicePtr, nonUsmHostPtr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(nonUsmHostPtr, 1024, cpuMemCopyInfo.srcAllocData);
    EXPECT_FALSE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, cpuMemCopyInfo.dstAllocData);
    EXPECT_TRUE(dstFound);
    EXPECT_FALSE(cmdList.isSuitableUSMDeviceAlloc(cpuMemCopyInfo.srcAllocData));
    EXPECT_TRUE(cmdList.isSuitableUSMDeviceAlloc(cpuMemCopyInfo.dstAllocData));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenIsSuitableUSMHostAllocThenReturnCorrectValue) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    NEO::SvmAllocationData *srcAllocData;
    NEO::SvmAllocationData *dstAllocData;
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(hostPtr, 1024, srcAllocData);
    EXPECT_TRUE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, dstAllocData);
    EXPECT_TRUE(dstFound);
    EXPECT_TRUE(cmdList.isSuitableUSMHostAlloc(srcAllocData));
    EXPECT_FALSE(cmdList.isSuitableUSMHostAlloc(dstAllocData));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenIsSuitableUSMSharedAllocThenReturnCorrectValue) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    NEO::SvmAllocationData *hostAllocData;
    NEO::SvmAllocationData *deviceAllocData;
    NEO::SvmAllocationData *sharedAllocData;
    auto hostAllocFound = device->getDriverHandle()->findAllocationDataForRange(hostPtr, 1024, hostAllocData);
    EXPECT_TRUE(hostAllocFound);
    auto deviceAllocFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, deviceAllocData);
    EXPECT_TRUE(deviceAllocFound);
    auto sharedAllocFound = device->getDriverHandle()->findAllocationDataForRange(sharedPtr, 1024, sharedAllocData);
    EXPECT_TRUE(sharedAllocFound);
    EXPECT_FALSE(cmdList.isSuitableUSMSharedAlloc(hostAllocData));
    EXPECT_FALSE(cmdList.isSuitableUSMSharedAlloc(deviceAllocData));
    EXPECT_TRUE(cmdList.isSuitableUSMSharedAlloc(sharedAllocData));
}

struct LocalMemoryMultiSubDeviceFixture : public SingleRootMultiSubDeviceFixture {
    void setUp() {
        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.EnableImplicitScaling.set(1);
        SingleRootMultiSubDeviceFixture::setUp();
    }
    DebugManagerStateRestore restore;
};

using LocalMemoryMultiSubDeviceTest = Test<LocalMemoryMultiSubDeviceFixture>;

HWTEST_F(LocalMemoryMultiSubDeviceTest, givenImmediateCommandListWhenIsSuitableUSMDeviceAllocWithColouredBufferThenReturnFalse) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *devicePtr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(), &deviceDesc, 2 * MemoryConstants::megaByte, 1u, &devicePtr);

    NEO::SvmAllocationData *allocData;
    auto allocFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 2 * MemoryConstants::megaByte, allocData);
    EXPECT_TRUE(allocFound);
    EXPECT_FALSE(cmdList.isSuitableUSMDeviceAlloc(allocData));
    context->freeMem(devicePtr);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenCreatingThenCopyThroughLockedPtrEnabledIsSetCorrectly) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());
    EXPECT_EQ(whiteBoxCmdList->copyThroughLockedPtrEnabled, device->getGfxCoreHelper().copyThroughLockedPtrEnabled(device->getHwInfo(), device->getProductHelper()));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndForcingLockPtrViaEnvVariableWhenPreferCopyThroughLockPointerCalledThenTrueIsReturned) {
    debugManager.flags.ExperimentalForceCopyThroughLock.set(1);
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    CpuMemCopyInfo cpuMemCopyInfo(devicePtr, nonUsmHostPtr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(nonUsmHostPtr, 1024, cpuMemCopyInfo.srcAllocData);
    ASSERT_FALSE(srcFound);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, cpuMemCopyInfo.dstAllocData);
    ASSERT_TRUE(dstFound);
    EXPECT_TRUE(cmdList.preferCopyThroughLockedPtr(cpuMemCopyInfo, 0, nullptr));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenGetTransferTypeThenReturnCorrectValue) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *hostPtr2 = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, sz, 1u, &hostPtr2);
    EXPECT_NE(nullptr, hostPtr2);

    void *importedPtr = malloc(sz);
    EXPECT_NE(nullptr, importedPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getDriverHandle()->importExternalPointer(importedPtr, sz));

    NEO::SvmAllocationData *hostUSMAllocData;
    NEO::SvmAllocationData *hostNonUSMAllocData;
    NEO::SvmAllocationData *deviceUSMAllocData;
    NEO::SvmAllocationData *sharedUSMAllocData;
    NEO::SvmAllocationData *notSpecifiedAllocData;

    const auto hostUSMFound = device->getDriverHandle()->findAllocationDataForRange(hostPtr, 1024, hostUSMAllocData);
    EXPECT_TRUE(hostUSMFound);
    const auto hostNonUSMFound = device->getDriverHandle()->findAllocationDataForRange(nonUsmHostPtr, 1024, hostNonUSMAllocData);
    EXPECT_FALSE(hostNonUSMFound);
    const auto deviceUSMFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, deviceUSMAllocData);
    EXPECT_TRUE(deviceUSMFound);
    const auto sharedUSMFound = device->getDriverHandle()->findAllocationDataForRange(sharedPtr, 1024, sharedUSMAllocData);
    EXPECT_TRUE(sharedUSMFound);
    const auto hostUSM2Found = device->getDriverHandle()->findAllocationDataForRange(hostPtr2, 1024, notSpecifiedAllocData);
    EXPECT_TRUE(hostUSM2Found);

    notSpecifiedAllocData->memoryType = InternalMemoryType::notSpecified;
    CpuMemCopyInfo copyInfoHostNonUsmToNotSpecified(hostPtr2, nonUsmHostPtr, 1024);
    copyInfoHostNonUsmToNotSpecified.dstAllocData = notSpecifiedAllocData;
    copyInfoHostNonUsmToNotSpecified.srcAllocData = hostNonUSMAllocData;
    EXPECT_EQ(TransferType::unknown, cmdList.getTransferType(copyInfoHostNonUsmToNotSpecified));

    CpuMemCopyInfo copyInfoHostNonUsmToHostUsm(hostPtr, nonUsmHostPtr, 1024);
    copyInfoHostNonUsmToHostUsm.dstAllocData = hostUSMAllocData;
    copyInfoHostNonUsmToHostUsm.srcAllocData = hostNonUSMAllocData;
    EXPECT_EQ(TransferType::hostNonUsmToHostUsm, cmdList.getTransferType(copyInfoHostNonUsmToHostUsm));

    CpuMemCopyInfo copyInfoHostNonUsmToDeviceUsm(devicePtr, nonUsmHostPtr, 1024);
    copyInfoHostNonUsmToDeviceUsm.dstAllocData = deviceUSMAllocData;
    copyInfoHostNonUsmToDeviceUsm.srcAllocData = hostNonUSMAllocData;
    EXPECT_EQ(TransferType::hostNonUsmToDeviceUsm, cmdList.getTransferType(copyInfoHostNonUsmToDeviceUsm));

    CpuMemCopyInfo copyInfoHostNonUsmToSharedUsm(sharedPtr, nonUsmHostPtr, 1024);
    copyInfoHostNonUsmToSharedUsm.dstAllocData = sharedUSMAllocData;
    copyInfoHostNonUsmToSharedUsm.srcAllocData = hostNonUSMAllocData;
    EXPECT_EQ(TransferType::hostNonUsmToSharedUsm, cmdList.getTransferType(copyInfoHostNonUsmToSharedUsm));

    CpuMemCopyInfo copyInfoHostNonUsmToHostNonUsm(nonUsmHostPtr, nonUsmHostPtr, 1024);
    copyInfoHostNonUsmToHostNonUsm.dstAllocData = hostNonUSMAllocData;
    copyInfoHostNonUsmToHostNonUsm.srcAllocData = hostNonUSMAllocData;
    EXPECT_EQ(TransferType::hostNonUsmToHostNonUsm, cmdList.getTransferType(copyInfoHostNonUsmToHostNonUsm));

    CpuMemCopyInfo copyInfoHostNonUsmToHostImported(importedPtr, nonUsmHostPtr, 1024);
    copyInfoHostNonUsmToHostImported.dstIsImportedHostPtr = true;
    copyInfoHostNonUsmToHostImported.dstAllocData = nullptr;
    copyInfoHostNonUsmToHostImported.srcAllocData = hostNonUSMAllocData;
    EXPECT_EQ(TransferType::hostNonUsmToHostUsm, cmdList.getTransferType(copyInfoHostNonUsmToHostImported));

    CpuMemCopyInfo copyInfoHostImportedToHostUsm(hostPtr, importedPtr, 1024);
    copyInfoHostImportedToHostUsm.dstAllocData = hostUSMAllocData;
    copyInfoHostImportedToHostUsm.srcIsImportedHostPtr = true;
    copyInfoHostImportedToHostUsm.srcAllocData = nullptr;
    EXPECT_EQ(TransferType::hostUsmToHostUsm, cmdList.getTransferType(copyInfoHostImportedToHostUsm));

    CpuMemCopyInfo copyInfoHostImportedToDeviceUsm(devicePtr, importedPtr, 1024);
    copyInfoHostImportedToDeviceUsm.dstAllocData = deviceUSMAllocData;
    copyInfoHostImportedToDeviceUsm.srcIsImportedHostPtr = true;
    copyInfoHostImportedToDeviceUsm.srcAllocData = nullptr;
    EXPECT_EQ(TransferType::hostUsmToDeviceUsm, cmdList.getTransferType(copyInfoHostImportedToDeviceUsm));

    CpuMemCopyInfo copyInfoHostImportedToSharedUsm(sharedPtr, importedPtr, 1024);
    copyInfoHostImportedToSharedUsm.dstAllocData = sharedUSMAllocData;
    copyInfoHostImportedToSharedUsm.srcIsImportedHostPtr = true;
    copyInfoHostImportedToSharedUsm.srcAllocData = nullptr;
    EXPECT_EQ(TransferType::hostUsmToSharedUsm, cmdList.getTransferType(copyInfoHostImportedToSharedUsm));

    CpuMemCopyInfo copyInfoHostImportedToHostNonUsm(nonUsmHostPtr, importedPtr, 1024);
    copyInfoHostImportedToHostNonUsm.dstAllocData = hostNonUSMAllocData;
    copyInfoHostImportedToHostNonUsm.srcIsImportedHostPtr = true;
    copyInfoHostImportedToHostNonUsm.srcAllocData = nullptr;
    EXPECT_EQ(TransferType::hostUsmToHostNonUsm, cmdList.getTransferType(copyInfoHostImportedToHostNonUsm));

    CpuMemCopyInfo copyInfoHostImportedToHostImported(importedPtr, importedPtr, 1024);
    copyInfoHostImportedToHostImported.dstIsImportedHostPtr = true;
    copyInfoHostImportedToHostImported.dstAllocData = nullptr;
    copyInfoHostImportedToHostImported.srcIsImportedHostPtr = true;
    copyInfoHostImportedToHostImported.srcAllocData = nullptr;
    EXPECT_EQ(TransferType::hostUsmToHostUsm, cmdList.getTransferType(copyInfoHostImportedToHostImported));

    CpuMemCopyInfo copyInfoHostUsmToHostUsm(hostPtr, hostPtr, 1024);
    copyInfoHostUsmToHostUsm.dstAllocData = hostUSMAllocData;
    copyInfoHostUsmToHostUsm.srcAllocData = hostUSMAllocData;
    EXPECT_EQ(TransferType::hostUsmToHostUsm, cmdList.getTransferType(copyInfoHostUsmToHostUsm));

    CpuMemCopyInfo copyInfoHostUsmToDeviceUsm(devicePtr, hostPtr, 1024);
    copyInfoHostUsmToDeviceUsm.dstAllocData = deviceUSMAllocData;
    copyInfoHostUsmToDeviceUsm.srcAllocData = hostUSMAllocData;
    EXPECT_EQ(TransferType::hostUsmToDeviceUsm, cmdList.getTransferType(copyInfoHostUsmToDeviceUsm));

    CpuMemCopyInfo copyInfoHostUsmToSharedUsm(sharedPtr, hostPtr, 1024);
    copyInfoHostUsmToSharedUsm.dstAllocData = sharedUSMAllocData;
    copyInfoHostUsmToSharedUsm.srcAllocData = hostUSMAllocData;
    EXPECT_EQ(TransferType::hostUsmToSharedUsm, cmdList.getTransferType(copyInfoHostUsmToSharedUsm));

    CpuMemCopyInfo copyInfoHostUsmToHostNonUsm(nonUsmHostPtr, hostPtr, 1024);
    copyInfoHostUsmToHostNonUsm.dstAllocData = hostNonUSMAllocData;
    copyInfoHostUsmToHostNonUsm.srcAllocData = hostUSMAllocData;
    EXPECT_EQ(TransferType::hostUsmToHostNonUsm, cmdList.getTransferType(copyInfoHostUsmToHostNonUsm));

    CpuMemCopyInfo copyInfoHostUsmToHostImported(importedPtr, hostPtr, 1024);
    copyInfoHostUsmToHostImported.dstIsImportedHostPtr = true;
    copyInfoHostUsmToHostImported.dstAllocData = nullptr;
    copyInfoHostUsmToHostImported.srcAllocData = hostUSMAllocData;
    EXPECT_EQ(TransferType::hostUsmToHostUsm, cmdList.getTransferType(copyInfoHostUsmToHostImported));

    CpuMemCopyInfo copyInfoDeviceUsmToHostUsm(hostPtr, devicePtr, 1024);
    copyInfoDeviceUsmToHostUsm.dstAllocData = hostUSMAllocData;
    copyInfoDeviceUsmToHostUsm.srcAllocData = deviceUSMAllocData;
    EXPECT_EQ(TransferType::deviceUsmToHostUsm, cmdList.getTransferType(copyInfoDeviceUsmToHostUsm));

    CpuMemCopyInfo copyInfoDeviceUsmToDeviceUsm(devicePtr, devicePtr, 1024);
    copyInfoDeviceUsmToDeviceUsm.dstAllocData = deviceUSMAllocData;
    copyInfoDeviceUsmToDeviceUsm.srcAllocData = deviceUSMAllocData;
    EXPECT_EQ(TransferType::deviceUsmToDeviceUsm, cmdList.getTransferType(copyInfoDeviceUsmToDeviceUsm));

    CpuMemCopyInfo copyInfoDeviceUsmToSharedUsm(sharedPtr, devicePtr, 1024);
    copyInfoDeviceUsmToSharedUsm.dstAllocData = sharedUSMAllocData;
    copyInfoDeviceUsmToSharedUsm.srcAllocData = deviceUSMAllocData;
    EXPECT_EQ(TransferType::deviceUsmToSharedUsm, cmdList.getTransferType(copyInfoDeviceUsmToSharedUsm));

    CpuMemCopyInfo copyInfoDeviceUsmToHostNonUsm(nonUsmHostPtr, devicePtr, 1024);
    copyInfoDeviceUsmToHostNonUsm.dstAllocData = hostNonUSMAllocData;
    copyInfoDeviceUsmToHostNonUsm.srcAllocData = deviceUSMAllocData;
    EXPECT_EQ(TransferType::deviceUsmToHostNonUsm, cmdList.getTransferType(copyInfoDeviceUsmToHostNonUsm));

    CpuMemCopyInfo copyInfoDeviceUsmToHostImported(importedPtr, devicePtr, 1024);
    copyInfoDeviceUsmToHostImported.dstIsImportedHostPtr = true;
    copyInfoDeviceUsmToHostImported.dstAllocData = nullptr;
    copyInfoDeviceUsmToHostImported.srcAllocData = deviceUSMAllocData;
    EXPECT_EQ(TransferType::deviceUsmToHostUsm, cmdList.getTransferType(copyInfoDeviceUsmToHostImported));

    CpuMemCopyInfo copyInfoSharedUsmToHostUsm(hostPtr, sharedPtr, 1024);
    copyInfoSharedUsmToHostUsm.dstAllocData = hostUSMAllocData;
    copyInfoSharedUsmToHostUsm.srcAllocData = sharedUSMAllocData;
    EXPECT_EQ(TransferType::sharedUsmToHostUsm, cmdList.getTransferType(copyInfoSharedUsmToHostUsm));

    CpuMemCopyInfo copyInfoSharedUsmToDeviceUsm(devicePtr, sharedPtr, 1024);
    copyInfoSharedUsmToDeviceUsm.dstAllocData = deviceUSMAllocData;
    copyInfoSharedUsmToDeviceUsm.srcAllocData = sharedUSMAllocData;
    EXPECT_EQ(TransferType::sharedUsmToDeviceUsm, cmdList.getTransferType(copyInfoSharedUsmToDeviceUsm));

    CpuMemCopyInfo copyInfoSharedUsmToSharedUsm(sharedPtr, sharedPtr, 1024);
    copyInfoSharedUsmToSharedUsm.dstAllocData = sharedUSMAllocData;
    copyInfoSharedUsmToSharedUsm.srcAllocData = sharedUSMAllocData;
    EXPECT_EQ(TransferType::sharedUsmToSharedUsm, cmdList.getTransferType(copyInfoSharedUsmToSharedUsm));

    CpuMemCopyInfo copyInfoSharedUsmToHostNonUsm(nonUsmHostPtr, sharedPtr, 1024);
    copyInfoSharedUsmToHostNonUsm.dstAllocData = hostNonUSMAllocData;
    copyInfoSharedUsmToHostNonUsm.srcAllocData = sharedUSMAllocData;
    EXPECT_EQ(TransferType::sharedUsmToHostNonUsm, cmdList.getTransferType(copyInfoSharedUsmToHostNonUsm));

    CpuMemCopyInfo copyInfoSharedUsmToHostImported(importedPtr, sharedPtr, 1024);
    copyInfoSharedUsmToHostImported.dstIsImportedHostPtr = true;
    copyInfoSharedUsmToHostImported.dstAllocData = nullptr;
    copyInfoSharedUsmToHostImported.srcAllocData = sharedUSMAllocData;
    EXPECT_EQ(TransferType::sharedUsmToHostUsm, cmdList.getTransferType(copyInfoSharedUsmToHostImported));

    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getDriverHandle()->releaseImportedPointer(importedPtr));
    free(importedPtr);
    context->freeMem(hostPtr2);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenGetTransferThresholdThenReturnCorrectValue) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(0u, cmdList.getTransferThreshold(TransferType::unknown));

    EXPECT_EQ(1 * MemoryConstants::megaByte, cmdList.getTransferThreshold(TransferType::hostNonUsmToHostUsm));
    EXPECT_EQ(4 * MemoryConstants::megaByte, cmdList.getTransferThreshold(TransferType::hostNonUsmToDeviceUsm));
    EXPECT_EQ(0u, cmdList.getTransferThreshold(TransferType::hostNonUsmToSharedUsm));
    EXPECT_EQ(1 * MemoryConstants::megaByte, cmdList.getTransferThreshold(TransferType::hostNonUsmToHostNonUsm));

    EXPECT_EQ(200 * MemoryConstants::kiloByte, cmdList.getTransferThreshold(TransferType::hostUsmToHostUsm));
    EXPECT_EQ(50 * MemoryConstants::kiloByte, cmdList.getTransferThreshold(TransferType::hostUsmToDeviceUsm));
    EXPECT_EQ(0u, cmdList.getTransferThreshold(TransferType::hostUsmToSharedUsm));
    EXPECT_EQ(500 * MemoryConstants::kiloByte, cmdList.getTransferThreshold(TransferType::hostUsmToHostNonUsm));

    EXPECT_EQ(128u, cmdList.getTransferThreshold(TransferType::deviceUsmToHostUsm));
    EXPECT_EQ(0u, cmdList.getTransferThreshold(TransferType::deviceUsmToDeviceUsm));
    EXPECT_EQ(0u, cmdList.getTransferThreshold(TransferType::deviceUsmToSharedUsm));
    EXPECT_EQ(1 * MemoryConstants::kiloByte, cmdList.getTransferThreshold(TransferType::deviceUsmToHostNonUsm));

    EXPECT_EQ(0u, cmdList.getTransferThreshold(TransferType::sharedUsmToHostUsm));
    EXPECT_EQ(0u, cmdList.getTransferThreshold(TransferType::sharedUsmToDeviceUsm));
    EXPECT_EQ(0u, cmdList.getTransferThreshold(TransferType::sharedUsmToSharedUsm));
    EXPECT_EQ(0u, cmdList.getTransferThreshold(TransferType::sharedUsmToHostNonUsm));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndThresholdDebugFlagSetWhenGetTransferThresholdThenReturnCorrectValue) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_EQ(4 * MemoryConstants::megaByte, cmdList.getTransferThreshold(TransferType::hostNonUsmToDeviceUsm));
    EXPECT_EQ(1 * MemoryConstants::kiloByte, cmdList.getTransferThreshold(TransferType::deviceUsmToHostNonUsm));

    debugManager.flags.ExperimentalH2DCpuCopyThreshold.set(5 * MemoryConstants::megaByte);
    EXPECT_EQ(5 * MemoryConstants::megaByte, cmdList.getTransferThreshold(TransferType::hostNonUsmToDeviceUsm));

    debugManager.flags.ExperimentalD2HCpuCopyThreshold.set(6 * MemoryConstants::megaByte);
    EXPECT_EQ(6 * MemoryConstants::megaByte, cmdList.getTransferThreshold(TransferType::deviceUsmToHostNonUsm));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenCopyH2DThenLockPtr) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    EXPECT_EQ(nullptr, dstAlloc->getLockedPtr());
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
    EXPECT_NE(nullptr, dstAlloc->getLockedPtr());
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenCopyD2HThenLockPtr) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;

    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    EXPECT_EQ(nullptr, dstAlloc->getLockedPtr());
    cmdList.appendMemoryCopy(nonUsmHostPtr, devicePtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
    EXPECT_NE(nullptr, dstAlloc->getLockedPtr());
}

HWTEST_F(AppendMemoryLockedCopyTest, givenForceModeWhenCopyIsCalledThenBothAllocationsAreLocked) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ExperimentalForceCopyThroughLock.set(1);

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = false;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_device_mem_alloc_desc_t deviceDesc = {};
    void *devicePtr2 = nullptr;
    context->allocDeviceMem(device->toHandle(), &deviceDesc, sz, 1u, &devicePtr2);
    NEO::SvmAllocationData *allocData2;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr2, 1024, allocData2);
    auto dstAlloc2 = allocData2->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    EXPECT_EQ(nullptr, dstAlloc->getLockedPtr());
    EXPECT_EQ(nullptr, dstAlloc2->getLockedPtr());
    cmdList.appendMemoryCopy(devicePtr2, devicePtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(2u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
    EXPECT_NE(nullptr, dstAlloc->getLockedPtr());
    EXPECT_NE(nullptr, dstAlloc2->getLockedPtr());
    context->freeMem(devicePtr2);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenForceModeWhenCopyIsCalledFromHostUsmToDeviceUsmThenOnlyDeviceAllocationIsLocked) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ExperimentalForceCopyThroughLock.set(1);

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = false;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_host_mem_alloc_desc_t hostDesc = {};
    void *hostPtr = nullptr;
    context->allocHostMem(&hostDesc, sz, 1u, &hostPtr);
    NEO::SvmAllocationData *hostAlloc;
    device->getDriverHandle()->findAllocationDataForRange(hostPtr, 1024, hostAlloc);
    auto hostAlloction = hostAlloc->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    EXPECT_EQ(nullptr, dstAlloc->getLockedPtr());
    EXPECT_EQ(nullptr, hostAlloction->getLockedPtr());
    cmdList.appendMemoryCopy(hostPtr, devicePtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
    EXPECT_NE(nullptr, dstAlloc->getLockedPtr());
    EXPECT_EQ(nullptr, hostAlloction->getLockedPtr());
    context->freeMem(hostPtr);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenCopyH2DAndDstPtrLockedThenDontLockAgain) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    device->getDriverHandle()->getMemoryManager()->lockResource(dstAlloc);

    EXPECT_EQ(1u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenCopyH2DThenUseMemcpyAndReturnSuccess) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    memset(nonUsmHostPtr, 1, 1024);

    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    auto lockedPtr = reinterpret_cast<char *>(dstAlloc->getLockedPtr());
    EXPECT_EQ(0, memcmp(lockedPtr, nonUsmHostPtr, 1024));
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndSignalEventAndNonUsmHostPtrWhenCopyH2DThenSignalEvent) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_NOT_READY);
    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndSignalEventAndCpuMemcpyWhenGpuHangThenDontSynchronizeEvent) {
    ze_command_queue_desc_t desc = {};

    auto mockCmdQ = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getInternalEngine().commandStreamReceiver, &desc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = mockCmdQ.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdList.getCsr(false))->callBaseWaitForCompletionWithTimeout = false;
    reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdList.getCsr(false))->returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_NOT_READY);
    cmdList.appendBarrier(nullptr, 0, nullptr, false);
    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, event->toHandle(), 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_ERROR_DEVICE_LOST);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_NOT_READY);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenCpuMemcpyWithoutBarrierThenDontWaitForTagUpdate) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    uint32_t waitForFlushTagUpdateCalled = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdList.getCsr(false))->waitForCompletionWithTimeoutTaskCountCalled;
    EXPECT_EQ(waitForFlushTagUpdateCalled, 0u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenCpuMemcpyWithBarrierThenWaitForTagUpdate) {
    ze_command_queue_desc_t desc = {};

    auto mockCmdQ = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getInternalEngine().commandStreamReceiver, &desc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = mockCmdQ.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    cmdList.appendBarrier(nullptr, 0, nullptr, false);
    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    uint32_t waitForFlushTagUpdateCalled = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdList.getCsr(false))->waitForCompletionWithTimeoutTaskCountCalled;
    EXPECT_EQ(waitForFlushTagUpdateCalled, 1u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenAppendBarrierThenSetDependenciesPresent) {
    ze_command_queue_desc_t desc = {};

    auto mockCmdQ = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getInternalEngine().commandStreamReceiver, &desc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = mockCmdQ.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    EXPECT_FALSE(cmdList.dependenciesPresent);

    cmdList.appendBarrier(nullptr, 0, nullptr, false);

    EXPECT_TRUE(cmdList.dependenciesPresent);

    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_FALSE(cmdList.dependenciesPresent);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenAppendWaitOnEventsThenSetDependenciesPresent) {
    ze_command_queue_desc_t desc = {};

    auto mockCmdQ = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getInternalEngine().commandStreamReceiver, &desc);

    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.copyThroughLockedPtrEnabled = true;
    cmdList.cmdQImmediate = mockCmdQ.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    EXPECT_FALSE(cmdList.dependenciesPresent);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));
    auto eventHandle = event->toHandle();
    cmdList.appendWaitOnEvents(1, &eventHandle, nullptr, false, true, false, false, false, false);

    EXPECT_TRUE(cmdList.dependenciesPresent);

    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_FALSE(cmdList.dependenciesPresent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockAppendMemoryLockedCopyTestImmediateCmdList : public MockCommandListImmediateHw<gfxCoreFamily> {
  public:
    MockAppendMemoryLockedCopyTestImmediateCmdList() : MockCommandListImmediateHw<gfxCoreFamily>() {
        this->copyThroughLockedPtrEnabled = true;
    }
    ze_result_t appendMemoryCopyKernelWithGA(uintptr_t dstPtr, NEO::GraphicsAllocation *dstPtrAlloc,
                                             uint64_t dstOffset, uintptr_t srcPtr,
                                             NEO::GraphicsAllocation *srcPtrAlloc,
                                             uint64_t srcOffset, uint64_t size,
                                             uint64_t elementSize, Builtin builtin,
                                             L0::Event *signalEvent,
                                             bool isStateless,
                                             CmdListKernelLaunchParams &launchParams) override {
        appendMemoryCopyKernelWithGACalled++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendBarrier(ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) override {
        appendBarrierCalled++;
        return MockCommandListImmediateHw<gfxCoreFamily>::appendBarrier(hEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);
    }

    void synchronizeEventList(uint32_t numWaitEvents, ze_event_handle_t *waitEventList) override {
        synchronizeEventListCalled++;
        MockCommandListImmediateHw<gfxCoreFamily>::synchronizeEventList(numWaitEvents, waitEventList);
    }

    uint32_t synchronizeEventListCalled = 0;
    uint32_t appendBarrierCalled = 0;
    uint32_t appendMemoryCopyKernelWithGACalled = 0;
};

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndUsmSrcHostPtrWhenCopyH2DThenUseCpuMemcpy) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *usmSrcPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 1024, 1u, &usmSrcPtr);

    cmdList.appendMemoryCopy(devicePtr, usmSrcPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalled, 0u);
    context->freeMem(usmSrcPtr);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndUsmDstHostPtrWhenCopyThenUseGpuMemcpy) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *usmHostDstPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 1024, 1u, &usmHostDstPtr);

    cmdList.appendMemoryCopy(usmHostDstPtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_GE(cmdList.appendMemoryCopyKernelWithGACalled, 1u);
    context->freeMem(usmHostDstPtr);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndUsmSrcHostPtrWhenCopyThenUseGpuMemcpy) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    void *usmHostSrcPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 1024, 1u, &usmHostSrcPtr);

    cmdList.appendMemoryCopy(nonUsmHostPtr, usmHostSrcPtr, 1024, nullptr, 0, nullptr, copyParams);
    EXPECT_GE(cmdList.appendMemoryCopyKernelWithGACalled, 1u);
    context->freeMem(usmHostSrcPtr);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmSrcHostPtrWhenSizeTooLargeThenUseGpuMemcpy) {
    debugManager.flags.ExperimentalH2DCpuCopyThreshold.set(64);
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 128, nullptr, 0, nullptr, copyParams);
    EXPECT_GE(cmdList.appendMemoryCopyKernelWithGACalled, 1u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmDstHostPtrWhenSizeTooLargeThenUseGpuMemcpy) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.appendMemoryCopy(nonUsmHostPtr, devicePtr, 2 * MemoryConstants::kiloByte, nullptr, 0, nullptr, copyParams);
    EXPECT_GE(cmdList.appendMemoryCopyKernelWithGACalled, 1u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndFailedToLockPtrThenUseGpuMemcpy) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1 * MemoryConstants::megaByte, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(cmdList.appendMemoryCopyKernelWithGACalled, 0u);

    NEO::SvmAllocationData *dstAllocData;
    ASSERT_TRUE(device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1 * MemoryConstants::megaByte, dstAllocData));
    ASSERT_NE(dstAllocData, nullptr);
    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    auto graphicsAllocation = dstAllocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    mockMemoryManager->unlockResource(graphicsAllocation);
    mockMemoryManager->failLockResource = true;
    ASSERT_FALSE(graphicsAllocation->isLocked());

    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1 * MemoryConstants::megaByte, nullptr, 0, nullptr, copyParams);
    EXPECT_GT(cmdList.appendMemoryCopyKernelWithGACalled, 0u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndD2HCopyWhenSizeTooLargeButFlagSetThenUseCpuMemcpy) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    debugManager.flags.ExperimentalD2HCpuCopyThreshold.set(2048);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    cmdList.appendMemoryCopy(nonUsmHostPtr, devicePtr, 2 * MemoryConstants::kiloByte, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalled, 0u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndH2DCopyWhenSizeTooLargeButFlagSetThenUseCpuMemcpy) {
    debugManager.flags.ExperimentalH2DCpuCopyThreshold.set(3 * MemoryConstants::megaByte);
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 3 * MemoryConstants::megaByte, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalled, 0u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndCpuMemcpyWithDependencyThenAppendBarrierCalled) {
    ze_command_queue_desc_t desc = {};

    auto mockCmdQ = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getInternalEngine().commandStreamReceiver, &desc);

    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;

    cmdList.cmdQImmediate = mockCmdQ.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr uint32_t numEvents = 5;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = numEvents;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    std::unique_ptr<L0::Event> events[numEvents] = {};
    ze_event_handle_t waitlist[numEvents] = {};

    for (uint32_t i = 0; i < numEvents; i++) {
        events[i] = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));
        waitlist[i] = events[i]->toHandle();
    }

    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, nullptr, numEvents, waitlist, copyParams);
    EXPECT_EQ(cmdList.appendBarrierCalled, 1u);
    EXPECT_EQ(cmdList.synchronizeEventListCalled, 0u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndCpuMemcpyWithDependencyWithinThresholdThenWaitOnHost) {
    DebugManagerStateRestore restore;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQ = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getInternalEngine().commandStreamReceiver, &desc);

    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;

    cmdList.cmdQImmediate = mockCmdQ.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr uint32_t numEvents = 4;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = numEvents;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    std::unique_ptr<L0::Event> events[numEvents] = {};
    ze_event_handle_t waitlist[numEvents] = {};

    for (uint32_t i = 0; i < numEvents; i++) {
        events[i] = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));
        events[i]->hostSignal(false);
        waitlist[i] = events[i]->toHandle();
    }

    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, nullptr, numEvents, waitlist, copyParams);
    EXPECT_EQ(cmdList.appendBarrierCalled, 0u);
    EXPECT_EQ(cmdList.synchronizeEventListCalled, 1u);

    debugManager.flags.ExperimentalCopyThroughLockWaitlistSizeThreshold.set(numEvents - 1);

    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, nullptr, numEvents, waitlist, copyParams);
    EXPECT_EQ(cmdList.appendBarrierCalled, 1u);
    EXPECT_EQ(cmdList.synchronizeEventListCalled, 1u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndCpuMemcpyWithoutDependencyThenAppendBarrierNotCalled) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(cmdList.appendBarrierCalled, 0u);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndTimestampFlagSetWhenCpuMemcpyThenSetCorrectGpuTimestamps) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    neoDevice->setOSTime(new NEO::MockOSTimeWithConstTimestamp());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));
    auto phEvent = event->toHandle();
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, phEvent, 0, nullptr, copyParams);
    ze_kernel_timestamp_result_t resultTimestamp = {};
    auto result = event->queryKernelTimestamp(&resultTimestamp);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(resultTimestamp.context.kernelStart, NEO::MockDeviceTimeWithConstTimestamp::gpuTimestamp);
    EXPECT_EQ(resultTimestamp.global.kernelStart, NEO::MockDeviceTimeWithConstTimestamp::gpuTimestamp);
    EXPECT_EQ(resultTimestamp.context.kernelEnd, NEO::MockDeviceTimeWithConstTimestamp::gpuTimestamp + 1);
    EXPECT_EQ(resultTimestamp.global.kernelEnd, NEO::MockDeviceTimeWithConstTimestamp::gpuTimestamp + 1);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndTimestampFlagNotSetWhenCpuMemcpyThenDontSetGpuTimestamps) {
    struct MockGpuTimestampEvent : public EventImp<uint32_t> {
        using EventImp<uint32_t>::gpuStartTimestamp;
        using EventImp<uint32_t>::gpuEndTimestamp;
    };
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockAppendMemoryLockedCopyTestImmediateCmdList<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    neoDevice->setOSTime(new NEO::MockOSTimeWithConstTimestamp());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));
    auto phEvent = event->toHandle();
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, phEvent, 0, nullptr, copyParams);
    ze_kernel_timestamp_result_t resultTimestamp = {};
    auto result = event->queryKernelTimestamp(&resultTimestamp);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(0u, reinterpret_cast<MockGpuTimestampEvent *>(event.get())->gpuStartTimestamp);
    EXPECT_EQ(0u, reinterpret_cast<MockGpuTimestampEvent *>(event.get())->gpuEndTimestamp);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenAllocationDataWhenFailingToObtainLockedPtrFromDeviceThenNullptrIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    NEO::SvmAllocationData *dstAllocData = nullptr;
    EXPECT_TRUE(device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, dstAllocData));
    ASSERT_NE(dstAllocData, nullptr);
    auto graphicsAllocation = dstAllocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_FALSE(graphicsAllocation->isLocked());

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    mockMemoryManager->failLockResource = true;

    bool lockingFailed = false;
    void *lockedPtr = cmdList.obtainLockedPtrFromDevice(dstAllocData, devicePtr, lockingFailed);
    EXPECT_FALSE(graphicsAllocation->isLocked());
    EXPECT_TRUE(lockingFailed);
    EXPECT_EQ(lockedPtr, nullptr);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenNullAllocationDataWhenObtainLockedPtrFromDeviceCalledThenNullptrIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    bool lockingFailed = false;
    EXPECT_EQ(cmdList.obtainLockedPtrFromDevice(nullptr, devicePtr, lockingFailed), nullptr);
    EXPECT_FALSE(lockingFailed);
}

HWTEST_F(AppendMemoryLockedCopyTest, givenFailedToObtainLockedPtrWhenPerformingCpuMemoryCopyThenErrorIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    CpuMemCopyInfo cpuMemCopyInfo(nullptr, nullptr, 1024);
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(nonUsmHostPtr, 1024, cpuMemCopyInfo.srcAllocData);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, cpuMemCopyInfo.dstAllocData);
    ASSERT_TRUE(srcFound != dstFound);
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    mockMemoryManager->failLockResource = true;

    returnValue = cmdList.performCpuMemcpy(cpuMemCopyInfo, nullptr, 1, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, returnValue);

    std::swap(cpuMemCopyInfo.srcAllocData, cpuMemCopyInfo.dstAllocData);
    returnValue = cmdList.performCpuMemcpy(cpuMemCopyInfo, nullptr, 1, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, returnValue);
}

HWTEST_F(CommandListAppendLaunchKernel, givenUnalignePtrToFillWhenSettingFillPropertiesThenAllGroupsCountEqualSizeToFill) {
    createKernel();
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.device = device;
    auto unalignedOffset = 2u;
    auto patternSize = 8u;
    auto sizeToFill = 599u * patternSize;
    CmdListFillKernelArguments outArguments;
    cmdList.setupFillKernelArguments(unalignedOffset, patternSize, sizeToFill, outArguments, kernel.get());
    EXPECT_EQ(outArguments.groups * outArguments.mainGroupSize, sizeToFill);
}

HWTEST_F(CommandListAppendLaunchKernel, givenAlignePtrToFillWhenSettingFillPropertiesThenAllGroupsCountEqualSizeToFillDividedBySizeOfUint32) {
    createKernel();
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();
    cmdList.device = device;
    auto unalignedOffset = 4u;
    auto patternSize = 8u;
    auto sizeToFill = 599u * patternSize;
    CmdListFillKernelArguments outArguments;
    cmdList.setupFillKernelArguments(unalignedOffset, patternSize, sizeToFill, outArguments, kernel.get());
    EXPECT_EQ(outArguments.groups * outArguments.mainGroupSize, sizeToFill / sizeof(uint32_t));
}
template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListHwKernelSplit : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    ze_result_t appendLaunchKernelSplit(::L0::Kernel *kernel,
                                        const ze_group_count_t &threadGroupDimensions,
                                        ::L0::Event *event,
                                        CmdListKernelLaunchParams &launchParams) override {
        passedKernel = kernel;
        return status;
    }
    ze_result_t status = ZE_RESULT_SUCCESS;
    ::L0::Kernel *passedKernel;
};
HWTEST_F(CommandListAppendLaunchKernel, givenUnalignePtrToFillWhenAppendMemoryFillCalledThenRightLeftOverKernelIsDispatched) {
    auto commandList = std::make_unique<MockCommandListHwKernelSplit<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    auto unalignedOffset = 2u;
    uint32_t pattern = 0x12345678;
    auto patternSize = sizeof(pattern);
    auto sizeToFill = 599u * patternSize;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 0x1000, 0x1000, &dstBuffer);
    auto builtinKernelByte = device->getBuiltinFunctionsLib()->getFunction(Builtin::fillBufferRightLeftover);
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryFill(ptrOffset(dstBuffer, unalignedOffset), &pattern, patternSize, sizeToFill, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(commandList->passedKernel, builtinKernelByte);
    context->freeMem(dstBuffer);
}
HWTEST_F(CommandListAppendLaunchKernel, givenUnalignePtrToFillWhenKernelLaunchSplitForRighLeftoverKernelFailsThenFailedStatusIsReturned) {
    auto commandList = std::make_unique<MockCommandListHwKernelSplit<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    auto unalignedOffset = 2u;
    uint32_t pattern = 0x12345678;
    auto patternSize = sizeof(pattern);
    auto sizeToFill = 599u * patternSize;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 0x1000, 0x1000, &dstBuffer);
    auto builtinKernelByte = device->getBuiltinFunctionsLib()->getFunction(Builtin::fillBufferRightLeftover);
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryFill(ptrOffset(dstBuffer, unalignedOffset), &pattern, patternSize, sizeToFill, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(commandList->passedKernel, builtinKernelByte);
    context->freeMem(dstBuffer);
}
HWTEST_F(CommandListAppendLaunchKernel, givenUnalignePtrToFillWhenAppendMemoryFillCalledWithStatelessEnabledThenRightLeftOverStatelessKernelIsDispatched) {
    auto commandList = std::make_unique<MockCommandListHwKernelSplit<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    auto unalignedOffset = 2u;
    uint32_t pattern = 0x12345678;
    auto patternSize = sizeof(pattern);
    auto sizeToFill = 599u * patternSize;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 0x1000, 0x1000, &dstBuffer);
    commandList->status = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    CmdListMemoryCopyParams copyParams = {};
    copyParams.relaxedOrderingDispatch = true;
    auto ret = commandList->appendMemoryFill(ptrOffset(dstBuffer, unalignedOffset), &pattern, patternSize, sizeToFill, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    context->freeMem(dstBuffer);
}

HWTEST_F(CommandListAppendLaunchKernel, givenAlignePtrToFillWhenAppendMemoryFillCalledThenMiddleBufferKernelIsDispatched) {
    auto commandList = std::make_unique<MockCommandListHwKernelSplit<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    auto unalignedOffset = 4u;
    uint32_t pattern = 0x12345678;
    auto patternSize = sizeof(pattern);
    auto sizeToFill = 599u * patternSize;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 0x1000, 0x1000, &dstBuffer);
    auto builtinKernelByte = device->getBuiltinFunctionsLib()->getFunction(Builtin::fillBufferMiddle);
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryFill(ptrOffset(dstBuffer, unalignedOffset), &pattern, patternSize, sizeToFill, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(commandList->passedKernel, builtinKernelByte);
    context->freeMem(dstBuffer);
}

struct ImmediateCommandListHostSynchronize : public Test<DeviceFixture> {
    template <GFXCORE_FAMILY gfxCoreFamily>
    std::unique_ptr<MockCommandListImmediateHw<gfxCoreFamily>> createCmdList(CommandStreamReceiver *csr) {
        const ze_command_queue_desc_t desc = {};

        auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr, &desc);
        commandQueue->initialize(false, false, false);

        auto cmdList = std::make_unique<MockCommandListImmediateHw<gfxCoreFamily>>();
        cmdList->cmdQImmediate = commandQueue;
        cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
        cmdList->isSyncModeQueue = false;

        return cmdList;
    }
};

HWTEST_F(ImmediateCommandListHostSynchronize, givenCsrClientCountWhenCallingSynchronizeThenUnregister) {
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getInternalEngine().commandStreamReceiver);

    auto cmdList = createCmdList<FamilyType::gfxCoreFamily>(csr);

    cmdList->cmdQImmediate->registerCsrClient();

    auto clientCount = csr->getNumClients();

    EXPECT_EQ(cmdList->hostSynchronize(0), ZE_RESULT_SUCCESS);

    EXPECT_EQ(clientCount - 1, csr->getNumClients());

    EXPECT_EQ(cmdList->hostSynchronize(0), ZE_RESULT_SUCCESS);

    EXPECT_EQ(clientCount - 1, csr->getNumClients());

    cmdList->cmdQImmediate->registerCsrClient();

    clientCount = csr->getNumClients();

    csr->callBaseWaitForCompletionWithTimeout = false;
    csr->returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;

    EXPECT_EQ(cmdList->hostSynchronize(0), ZE_RESULT_ERROR_DEVICE_LOST);

    EXPECT_EQ(clientCount, csr->getNumClients());
}

HWTEST_F(ImmediateCommandListHostSynchronize, givenFlushTaskEnabledAndNotSyncModeThenWaitForCompletionIsCalled) {
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getInternalEngine().commandStreamReceiver);

    auto cmdList = createCmdList<FamilyType::gfxCoreFamily>(csr);

    EXPECT_EQ(cmdList->hostSynchronize(0), ZE_RESULT_SUCCESS);

    uint32_t waitForFlushTagUpdateCalled = csr->waitForCompletionWithTimeoutTaskCountCalled;
    EXPECT_EQ(waitForFlushTagUpdateCalled, 1u);
}

HWTEST_F(ImmediateCommandListHostSynchronize, givenSyncModeThenWaitForCompletionIsCalled) {
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getInternalEngine().commandStreamReceiver);

    auto cmdList = createCmdList<FamilyType::gfxCoreFamily>(csr);
    cmdList->isSyncModeQueue = true;

    EXPECT_EQ(cmdList->hostSynchronize(0), ZE_RESULT_SUCCESS);

    uint32_t waitForFlushTagUpdateCalled = csr->waitForCompletionWithTimeoutTaskCountCalled;
    EXPECT_EQ(waitForFlushTagUpdateCalled, 1u);
}

HWTEST_F(ImmediateCommandListHostSynchronize, givenFlushTaskSubmissionIsDisabledThenWaitForCompletionIsCalled) {
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getInternalEngine().commandStreamReceiver);

    auto cmdList = createCmdList<FamilyType::gfxCoreFamily>(csr);
    cmdList->copyThroughLockedPtrEnabled = true;

    EXPECT_EQ(cmdList->hostSynchronize(0), ZE_RESULT_SUCCESS);

    uint32_t waitForFlushTagUpdateCalled = csr->waitForCompletionWithTimeoutTaskCountCalled;
    EXPECT_EQ(waitForFlushTagUpdateCalled, 1u);
}

HWTEST_F(ImmediateCommandListHostSynchronize, givenGpuStatusIsHangThenDeviceLostIsReturned) {
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getInternalEngine().commandStreamReceiver);

    auto cmdList = createCmdList<FamilyType::gfxCoreFamily>(csr);

    csr->callBaseWaitForCompletionWithTimeout = false;
    csr->returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;

    EXPECT_EQ(cmdList->hostSynchronize(0), ZE_RESULT_ERROR_DEVICE_LOST);

    uint32_t waitForFlushTagUpdateCalled = csr->waitForCompletionWithTimeoutTaskCountCalled;
    EXPECT_EQ(waitForFlushTagUpdateCalled, 1u);
}

HWTEST_F(ImmediateCommandListHostSynchronize, givenTimeoutOtherThanMaxIsProvidedWaitParamsIsSetCorrectly) {
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getInternalEngine().commandStreamReceiver);

    auto cmdList = createCmdList<FamilyType::gfxCoreFamily>(csr);

    csr->callBaseWaitForCompletionWithTimeout = false;
    csr->returnWaitForCompletionWithTimeout = WaitStatus::ready;

    EXPECT_EQ(cmdList->hostSynchronize(1000), ZE_RESULT_SUCCESS);
    auto waitParams = csr->latestWaitForCompletionWithTimeoutWaitParams;
    EXPECT_TRUE(waitParams.enableTimeout);
    EXPECT_FALSE(waitParams.indefinitelyPoll);
    EXPECT_EQ(waitParams.waitTimeout, 1);
}

HWTEST_F(ImmediateCommandListHostSynchronize, givenMaxTimeoutIsProvidedWaitParamsIsSetCorrectly) {
    auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getInternalEngine().commandStreamReceiver);

    auto cmdList = createCmdList<FamilyType::gfxCoreFamily>(csr);

    csr->callBaseWaitForCompletionWithTimeout = false;
    csr->returnWaitForCompletionWithTimeout = WaitStatus::ready;

    EXPECT_EQ(cmdList->hostSynchronize(std::numeric_limits<uint64_t>::max()), ZE_RESULT_SUCCESS);

    auto waitParams = csr->latestWaitForCompletionWithTimeoutWaitParams;
    EXPECT_FALSE(waitParams.enableTimeout);
    EXPECT_TRUE(waitParams.indefinitelyPoll);
}

using CommandListHostSynchronize = Test<DeviceFixture>;

HWTEST_F(CommandListHostSynchronize, whenHostSychronizeIsCalledReturnInvalidArgument) {
    ze_command_list_desc_t desc = {};
    ze_command_list_handle_t hCommandList = {};

    ze_result_t result = context->createCommandList(device, &desc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(Context::fromHandle(CommandList::fromHandle(hCommandList)->getCmdListContext()), context);

    L0::CommandList *commandList = L0::CommandList::fromHandle(hCommandList);
    EXPECT_EQ(commandList->hostSynchronize(0), ZE_RESULT_ERROR_INVALID_ARGUMENT);
    commandList->destroy();
}

using CommandListMappedTimestampTest = CommandListAppendLaunchKernel;

HWTEST_F(CommandListMappedTimestampTest, givenMappedTimestampSignalEventWhenAppendApiIsCalledThenTheEventIsAddedToMappedSignalList) {
    createKernel();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ze_result_t returnValue;
    std::unique_ptr<L0::EventPool> eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    ze_group_count_t groupCount{1, 1, 1};

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, cooperativeParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(event.get(), commandList->peekMappedEventList()[0]);
}

HWTEST_F(CommandListMappedTimestampTest, givenSignalEventWithoutMappedTimstampWhenAppendApiIsCalledThenTheEventIsNotAddedToMappedSignalList) {
    createKernel();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ze_result_t returnValue;
    std::unique_ptr<L0::EventPool> eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    ze_group_count_t groupCount{1, 1, 1};

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, cooperativeParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(0u, commandList->peekMappedEventList().size());
}

HWTEST_F(CommandListMappedTimestampTest, givenMappedTimestampSignalEventWhenAppendApiIsCalledMultipleTimesThenTheEventIsAddedOnceToMappedSignalList) {
    createKernel();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ze_result_t returnValue;
    std::unique_ptr<L0::EventPool> eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    ze_group_count_t groupCount{1, 1, 1};

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, cooperativeParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList->appendBarrier(event->toHandle(), 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(event.get(), commandList->peekMappedEventList()[0]);
    EXPECT_EQ(1u, commandList->peekMappedEventList().size());
}

HWTEST_F(CommandListMappedTimestampTest, givenEventIsAddedToMappedEventListWhenStoringReferenceTimestampWithClearThenEventsAreCleared) {
    createKernel();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ze_result_t returnValue;
    std::unique_ptr<L0::EventPool> eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    std::unique_ptr<L0::Event> event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue));

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    commandList->addToMappedEventList(event.get());
    commandList->storeReferenceTsToMappedEvents(false);
    EXPECT_EQ(1u, commandList->peekMappedEventList().size());
    commandList->storeReferenceTsToMappedEvents(true);
    EXPECT_EQ(0u, commandList->peekMappedEventList().size());
}

template <GFXCORE_FAMILY gfxCoreFamily, typename BaseMock>
class MockCommandListCoreFamilyIfPrivateNeeded : public BaseMock {
  public:
    void allocateOrReuseKernelPrivateMemory(Kernel *kernel, uint32_t sizePerHwThread, PrivateAllocsToReuseContainer &privateAllocsToReuse) override {
        passedContainer = &privateAllocsToReuse;
        BaseMock::allocateOrReuseKernelPrivateMemory(kernel, sizePerHwThread, privateAllocsToReuse);
    }
    PrivateAllocsToReuseContainer *passedContainer;
};

HWTEST_F(CommandListCreate, givenPrivatePerDispatchDisabledWhenAllocatingPrivateMemoryThenAllocateIsNotCalled) {
    auto commandList = std::make_unique<MockCommandListCoreFamilyIfPrivateNeeded<FamilyType::gfxCoreFamily, MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->allocateOrReuseKernelPrivateMemoryIfNeededCallBase = true;
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;
    mockModule.allocatePrivateMemoryPerDispatch = false;
    commandList->allocateOrReuseKernelPrivateMemoryIfNeeded(&mockKernel, 0x1000);
    EXPECT_EQ(commandList->allocateOrReuseKernelPrivateMemoryCalled, 0u);
}

HWTEST_F(CommandListCreate, givenPrivatePerDispatchEnabledWhenAllocatingPrivateMemoryThenAllocateIsCalled) {
    auto commandList = std::make_unique<MockCommandListCoreFamilyIfPrivateNeeded<FamilyType::gfxCoreFamily, MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->allocateOrReuseKernelPrivateMemoryIfNeededCallBase = true;
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;
    mockModule.allocatePrivateMemoryPerDispatch = true;
    commandList->allocateOrReuseKernelPrivateMemoryIfNeeded(&mockKernel, 0x1000);
    EXPECT_EQ(commandList->allocateOrReuseKernelPrivateMemoryCalled, 1u);
}

HWTEST_F(CommandListCreate, givenPrivatePerDispatchEnabledWhenAllocatingPrivateMemoryThenCmdListMaprIsPassed) {
    auto commandList = std::make_unique<MockCommandListCoreFamilyIfPrivateNeeded<FamilyType::gfxCoreFamily, MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->allocateOrReuseKernelPrivateMemoryIfNeededCallBase = true;
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;
    mockModule.allocatePrivateMemoryPerDispatch = true;
    commandList->allocateOrReuseKernelPrivateMemoryIfNeeded(&mockKernel, 0x1000);
    EXPECT_EQ(commandList->passedContainer, &commandList->ownedPrivateAllocations);
}

HWTEST_F(CommandListCreate, givenImmediateListAndPrivatePerDispatchDisabledWhenAllocatingPrivateMemoryCalledThenAllocateIsNotCalled) {
    auto commandList = std::make_unique<MockCommandListCoreFamilyIfPrivateNeeded<FamilyType::gfxCoreFamily, MockCommandListImmediateHw<FamilyType::gfxCoreFamily>>>();
    commandList->allocateOrReuseKernelPrivateMemoryIfNeededCallBase = true;
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;
    mockModule.allocatePrivateMemoryPerDispatch = false;
    commandList->allocateOrReuseKernelPrivateMemoryIfNeeded(&mockKernel, 0x1000);
    EXPECT_EQ(commandList->allocateOrReuseKernelPrivateMemoryCalled, 0u);
}

HWTEST_F(CommandListCreate, givenImmediateListAndPrivatePerDispatchEnabledWhenAllocatingPrivateMemoryThenAllocateIsCalled) {
    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, &mockCommandStreamReceiver, &queueDesc);

    auto commandList = std::make_unique<MockCommandListCoreFamilyIfPrivateNeeded<FamilyType::gfxCoreFamily, MockCommandListImmediateHw<FamilyType::gfxCoreFamily>>>();
    commandList->cmdQImmediate = queue.get();
    commandList->allocateOrReuseKernelPrivateMemoryIfNeededCallBase = true;

    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;
    mockModule.allocatePrivateMemoryPerDispatch = true;
    commandList->allocateOrReuseKernelPrivateMemoryIfNeeded(&mockKernel, 0x1000);
    EXPECT_EQ(commandList->allocateOrReuseKernelPrivateMemoryCalled, 1u);
}

HWTEST_F(CommandListCreate, givenImmediateListAndPrivatePerDispatchEnabledWhenAllocatingPrivateMemoryThenCsrMapIsPassed) {
    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, &mockCommandStreamReceiver, &queueDesc);

    auto commandList = std::make_unique<MockCommandListCoreFamilyIfPrivateNeeded<FamilyType::gfxCoreFamily, MockCommandListImmediateHw<FamilyType::gfxCoreFamily>>>();
    commandList->allocateOrReuseKernelPrivateMemoryIfNeededCallBase = true;
    commandList->cmdQImmediate = queue.get();

    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    mockKernel.module = &mockModule;
    mockModule.allocatePrivateMemoryPerDispatch = true;
    commandList->allocateOrReuseKernelPrivateMemoryIfNeeded(&mockKernel, 0x1000);
    EXPECT_EQ(commandList->passedContainer, &mockCommandStreamReceiver.getOwnedPrivateAllocations());
}

HWTEST_F(CommandListCreate, givenCmdListWhenAllocateOrReuseCalledForSizeThatIsStoredInMapThenItsReused) {
    auto commandList = std::make_unique<MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->allocateOrReuseKernelPrivateMemoryCallBase = true;
    commandList->device = this->device;
    uint32_t sizePerHwThread = 0x1000;
    auto mockMem = std::make_unique<uint8_t[]>(0x1000);
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    const_cast<uint32_t &>(mockKernel.sharedState->kernelImmData->getDescriptor().kernelAttributes.perHwThreadPrivateMemorySize) = 0x1000;
    mockKernel.module = &mockModule;
    MockGraphicsAllocation mockGA(mockMem.get(), 2 * sizePerHwThread * this->neoDevice->getDeviceInfo().computeUnitsUsedForScratch);
    PrivateAllocsToReuseContainer mapForReuse;
    mapForReuse.push_back({sizePerHwThread, &mockGA});
    auto sizeBefore = mapForReuse.size();
    commandList->allocateOrReuseKernelPrivateMemory(&mockKernel, sizePerHwThread, mapForReuse);
    EXPECT_EQ(sizeBefore, mapForReuse.size());
}

HWTEST_F(CommandListCreate, givenNewSizeDifferentThanSizesInMapWhenAllocatingPrivateMemoryThenNewAllocationIsCreated) {
    auto commandList = std::make_unique<MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->allocateOrReuseKernelPrivateMemoryCallBase = true;
    commandList->device = this->device;
    uint32_t sizePerHwThread = 0x1000;
    auto mockMem = std::make_unique<uint8_t[]>(0x1000);
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    const_cast<uint32_t &>(mockKernel.sharedState->kernelImmData->getDescriptor().kernelAttributes.perHwThreadPrivateMemorySize) = sizePerHwThread;
    mockKernel.module = &mockModule;
    MockGraphicsAllocation mockGA(mockMem.get(), sizePerHwThread * this->neoDevice->getDeviceInfo().computeUnitsUsedForScratch / 2);
    PrivateAllocsToReuseContainer mapForReuse;
    mapForReuse.push_back({sizePerHwThread, &mockGA});
    auto sizeBefore = mapForReuse.size();
    commandList->allocateOrReuseKernelPrivateMemory(&mockKernel, sizePerHwThread / 2, mapForReuse);
    EXPECT_NE(sizeBefore, mapForReuse.size());
    neoDevice->getMemoryManager()->freeGraphicsMemory(commandList->commandContainer.getResidencyContainer()[0]);
}

HWTEST_F(CommandListCreate, givenNewSizeDifferentThanSizesInMapWhenAllocatingPrivateMemoryThenNewAllocationIsAddedToCommandContainerResidencyList) {
    auto commandList = std::make_unique<MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->allocateOrReuseKernelPrivateMemoryCallBase = true;
    commandList->device = this->device;
    uint32_t sizePerHwThread = 0x1000;
    auto mockMem = std::make_unique<uint8_t[]>(0x1000);
    Mock<Module> mockModule(this->device, nullptr);
    Mock<KernelImp> mockKernel;
    const_cast<uint32_t &>(mockKernel.sharedState->kernelImmData->getDescriptor().kernelAttributes.perHwThreadPrivateMemorySize) = sizePerHwThread;
    mockKernel.module = &mockModule;
    MockGraphicsAllocation mockGA(mockMem.get(), sizePerHwThread * this->neoDevice->getDeviceInfo().computeUnitsUsedForScratch / 2);
    PrivateAllocsToReuseContainer mapForReuse;
    mapForReuse.push_back({sizePerHwThread, &mockGA});
    auto sizeBefore = commandList->commandContainer.getResidencyContainer().size();
    commandList->allocateOrReuseKernelPrivateMemory(&mockKernel, sizePerHwThread / 2, mapForReuse);
    EXPECT_NE(sizeBefore, commandList->commandContainer.getResidencyContainer().size());
    neoDevice->getMemoryManager()->freeGraphicsMemory(commandList->commandContainer.getResidencyContainer()[0]);
}

HWTEST_F(CommandListCreate, givenCounterDeviceAllocWhenGetDeviceCounterAllocForResidencyThenReturnCorrectAllocation) {
    auto commandList = std::make_unique<MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    MockGraphicsAllocation counterDeviceAlloc(this->device->getRootDeviceIndex(), nullptr, 0x0);
    auto result = commandList->getDeviceCounterAllocForResidency(&counterDeviceAlloc);
    EXPECT_EQ(result, &counterDeviceAlloc);
}

HWTEST_F(CommandListCreate, givenCounterDeviceAllocFromDifferentRootDeviceWhenGetDeviceCounterAllocForResidencyThenReturnPeerAllocation) {
    auto commandList = std::make_unique<MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    uint32_t baseDeviceIndex = this->device->getRootDeviceIndex();
    uint32_t peerDeviceIndex = baseDeviceIndex + 1;

    MockGraphicsAllocation counterDeviceAlloc(baseDeviceIndex, reinterpret_cast<void *>(0x1234), 0x0u);
    MockGraphicsAllocation peerAllocation(peerDeviceIndex, reinterpret_cast<void *>(0x5678), 0x0u);
    ASSERT_NE(counterDeviceAlloc.getRootDeviceIndex(), peerAllocation.getRootDeviceIndex());
    ASSERT_NE(counterDeviceAlloc.getGpuAddress(), peerAllocation.getGpuAddress());

    auto result = commandList->getDeviceCounterAllocForResidency(&peerAllocation);
    EXPECT_EQ(result->getGpuAddress(), peerAllocation.getGpuAddress());
}

HWTEST_F(CommandListCreate, givenCounterDeviceAllocFromDifferentRootDeviceWhenGetDeviceCounterAllocForResidencyAndAllocIsAlreadyImportedThenReturnPeerAllocation) {
    auto commandList = std::make_unique<MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    uint32_t baseDeviceIndex = this->device->getRootDeviceIndex();
    uint32_t peerDeviceIndex = baseDeviceIndex + 1;

    MockGraphicsAllocation counterDeviceAlloc(baseDeviceIndex, reinterpret_cast<void *>(0x1234), 0x0u);
    MockGraphicsAllocation peerAllocation(peerDeviceIndex, reinterpret_cast<void *>(0x5678), 0x0u);
    ASSERT_NE(counterDeviceAlloc.getRootDeviceIndex(), peerAllocation.getRootDeviceIndex());
    ASSERT_NE(counterDeviceAlloc.getGpuAddress(), peerAllocation.getGpuAddress());

    auto deviceImp = static_cast<DeviceImp *>(this->device);
    NEO::SvmAllocationData allocData(peerDeviceIndex);
    allocData.gpuAllocations.addAllocation(&peerAllocation);
    deviceImp->peerCounterAllocations.allocations.insert({reinterpret_cast<void *>(peerAllocation.getGpuAddress()), allocData});

    auto result = commandList->getDeviceCounterAllocForResidency(&peerAllocation);
    EXPECT_EQ(result->getGpuAddress(), peerAllocation.getGpuAddress());

    deviceImp->peerCounterAllocations.remove(allocData);
}

HWTEST_F(CommandListCreate, givenNullptrPeerAllocationWhenGetDeviceCounterAllocForResidencyThenAbortIsThrown) {
    auto commandList = std::make_unique<MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    uint32_t baseDeviceIndex = this->device->getRootDeviceIndex();
    uint32_t peerDeviceIndex = baseDeviceIndex + 1;

    MockGraphicsAllocation counterDeviceAlloc(peerDeviceIndex, nullptr, 0x0);

    EXPECT_ANY_THROW(commandList->getDeviceCounterAllocForResidency(&counterDeviceAlloc));
}

HWTEST_F(CommandListCreate, givenCommandListWhenIsKernelUncachedMocsRequiredCalledThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableStateBaseAddressTracking.set(0);

    auto commandList = std::make_unique<MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_FALSE(commandList->isKernelUncachedMocsRequired(false));

    auto &rootDeviceEnvironment = neoDevice->executionEnvironment->rootDeviceEnvironments[0];
    auto deferMOCSToPatIndex = device->getProductHelper().deferMOCSToPatIndex(rootDeviceEnvironment->isWddmOnLinux());
    EXPECT_EQ(!deferMOCSToPatIndex, commandList->isKernelUncachedMocsRequired(true));
    EXPECT_TRUE(commandList->getContainsStatelessUncachedResource());
}

HWTEST_F(CommandListCreate, givenSbaTrackingEnabledWhenIsKernelUncachedMocsRequiredCalledThenReturnFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableStateBaseAddressTracking.set(1);

    auto commandList = std::make_unique<MockCommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    EXPECT_FALSE(commandList->isKernelUncachedMocsRequired(true));
    EXPECT_TRUE(commandList->getContainsStatelessUncachedResource());
}

} // namespace ult
} // namespace L0

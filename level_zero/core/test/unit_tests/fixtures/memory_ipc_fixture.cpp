/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/common/ult_helpers_l0.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

void *DriverHandleGetFdMock::importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAloc, NEO::SvmAllocationData &mappedPeerAllocData) {
    this->allocationTypeRequested = allocationType;
    if (mockFd == allocationMap.second) {
        return allocationMap.first;
    }
    return nullptr;
}

ze_result_t ContextFdMock::allocDeviceMem(ze_device_handle_t hDevice,
                                          const ze_device_mem_alloc_desc_t *deviceDesc,
                                          size_t size,
                                          size_t alignment, void **ptr) {
    ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
    if (ZE_RESULT_SUCCESS == res) {
        driverHandle->allocationMap.first = *ptr;
        driverHandle->allocationMap.second = driverHandle->mockFd;
    }

    return res;
}

ze_result_t ContextFdMock::allocHostMem(const ze_host_mem_alloc_desc_t *hostDesc,
                                        size_t size,
                                        size_t alignment, void **ptr) {
    ze_result_t res = L0::ContextImp::allocHostMem(hostDesc, size, alignment, ptr);
    if (ZE_RESULT_SUCCESS == res) {
        driverHandle->allocationMap.first = *ptr;
        driverHandle->allocationMap.second = driverHandle->mockFd;
    }

    return res;
}

ze_result_t ContextFdMock::getMemAllocProperties(const void *ptr,
                                                 ze_memory_allocation_properties_t *pMemAllocProperties,
                                                 ze_device_handle_t *phDevice) {
    ze_result_t res = ContextImp::getMemAllocProperties(ptr, pMemAllocProperties, phDevice);
    if (ZE_RESULT_SUCCESS == res && pMemAllocProperties->pNext && !memPropTest) {
        ze_base_properties_t *baseProperties =
            reinterpret_cast<ze_base_properties_t *>(pMemAllocProperties->pNext);
        if (baseProperties->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD) {
            ze_external_memory_export_fd_t *extendedMemoryExportProperties =
                reinterpret_cast<ze_external_memory_export_fd_t *>(pMemAllocProperties->pNext);
            extendedMemoryExportProperties->fd = driverHandle->mockFd;
        }
    }

    return res;
}

ze_result_t ContextFdMock::getImageAllocProperties(Image *image,
                                                   ze_image_allocation_ext_properties_t *pAllocProperties) {

    ze_result_t res = ContextImp::getImageAllocProperties(image, pAllocProperties);
    if (ZE_RESULT_SUCCESS == res && pAllocProperties->pNext) {
        ze_base_properties_t *baseProperties =
            reinterpret_cast<ze_base_properties_t *>(pAllocProperties->pNext);
        if (baseProperties->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD) {
            ze_external_memory_export_fd_t *extendedMemoryExportProperties =
                reinterpret_cast<ze_external_memory_export_fd_t *>(pAllocProperties->pNext);
            extendedMemoryExportProperties->fd = driverHandle->mockFd;
        }
    }

    return res;
}

void MemoryExportImportTest::SetUp() {

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<DriverHandleGetFdMock>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    context = std::make_unique<ContextFdMock>(driverHandle.get());
    EXPECT_NE(context, nullptr);
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
}

void *DriverHandleGetMemHandleMock::importNTHandle(ze_device_handle_t hDevice, void *handle, NEO::AllocationType allocationType, uint32_t parentProcessId) {
    if (mockHandle == allocationHandleMap.second) {
        return allocationHandleMap.first;
    }
    return nullptr;
}
void *DriverHandleGetMemHandleMock::importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle,
                                                   NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAloc, NEO::SvmAllocationData &mappedPeerAllocData) {
    if (mockFd == allocationFdMap.second) {
        return allocationFdMap.first;
    }
    return nullptr;
}

ze_result_t ContextMemHandleMock::allocDeviceMem(ze_device_handle_t hDevice,
                                                 const ze_device_mem_alloc_desc_t *deviceDesc,
                                                 size_t size,
                                                 size_t alignment, void **ptr) {
    ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
    if (ZE_RESULT_SUCCESS == res) {
        driverHandle->allocationFdMap.first = *ptr;
        driverHandle->allocationFdMap.second = driverHandle->mockFd;
        driverHandle->allocationHandleMap.first = *ptr;
        driverHandle->allocationHandleMap.second = driverHandle->mockHandle;
    }

    return res;
}

ze_result_t ContextMemHandleMock::getMemAllocProperties(const void *ptr,
                                                        ze_memory_allocation_properties_t *pMemAllocProperties,
                                                        ze_device_handle_t *phDevice) {
    ze_result_t res = ContextImp::getMemAllocProperties(ptr, pMemAllocProperties, phDevice);
    if (ZE_RESULT_SUCCESS == res && pMemAllocProperties->pNext) {
        ze_base_properties_t *baseProperties =
            reinterpret_cast<ze_base_properties_t *>(pMemAllocProperties->pNext);
        if (baseProperties->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD) {
            ze_external_memory_export_fd_t *extendedMemoryExportProperties =
                reinterpret_cast<ze_external_memory_export_fd_t *>(pMemAllocProperties->pNext);
            extendedMemoryExportProperties->fd = driverHandle->mockFd;
        }
    }

    return res;
}

ze_result_t ContextMemHandleMock::getImageAllocProperties(Image *image,
                                                          ze_image_allocation_ext_properties_t *pAllocProperties) {

    ze_result_t res = ContextImp::getImageAllocProperties(image, pAllocProperties);
    if (ZE_RESULT_SUCCESS == res && pAllocProperties->pNext) {
        ze_base_properties_t *baseProperties =
            reinterpret_cast<ze_base_properties_t *>(pAllocProperties->pNext);
        if (baseProperties->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD) {
            ze_external_memory_export_fd_t *extendedMemoryExportProperties =
                reinterpret_cast<ze_external_memory_export_fd_t *>(pAllocProperties->pNext);
            extendedMemoryExportProperties->fd = driverHandle->mockFd;
        }
    }

    return res;
}

void MemoryExportImportWSLTest::SetUp() {

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<DriverHandleGetMemHandleMock>();
    prevMemoryManager = driverHandle->getMemoryManager();
    currMemoryManager = new MemoryManagerMemHandleMock();
    driverHandle->setMemoryManager(currMemoryManager);
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    context = std::make_unique<ContextMemHandleMock>(driverHandle.get());
    EXPECT_NE(context, nullptr);
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
}

void MemoryExportImportWSLTest::TearDown() {
    L0UltHelper::cleanupUsmAllocPoolsAndReuse(driverHandle.get());
    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;
}

void *DriverHandleGetWinHandleMock::importNTHandle(ze_device_handle_t hDevice, void *handle, NEO::AllocationType allocationType, uint32_t parentProcessId) {
    if (mockHandle == allocationMap.second) {
        return allocationMap.first;
    }
    return nullptr;
}

ze_result_t ContextHandleMock::allocDeviceMem(ze_device_handle_t hDevice,
                                              const ze_device_mem_alloc_desc_t *deviceDesc,
                                              size_t size,
                                              size_t alignment, void **ptr) {
    ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
    if (ZE_RESULT_SUCCESS == res) {
        driverHandle->allocationMap.first = *ptr;
        driverHandle->allocationMap.second = driverHandle->mockHandle;
    }

    return res;
}

ze_result_t ContextHandleMock::allocHostMem(const ze_host_mem_alloc_desc_t *hostDesc,
                                            size_t size,
                                            size_t alignment, void **ptr) {
    ze_result_t res = L0::ContextImp::allocHostMem(hostDesc, size, alignment, ptr);
    if (ZE_RESULT_SUCCESS == res) {
        driverHandle->allocationMap.first = *ptr;
        driverHandle->allocationMap.second = driverHandle->mockHandle;
    }

    return res;
}

ze_result_t ContextHandleMock::getMemAllocProperties(const void *ptr,
                                                     ze_memory_allocation_properties_t *pMemAllocProperties,
                                                     ze_device_handle_t *phDevice) {
    ze_result_t res = ContextImp::getMemAllocProperties(ptr, pMemAllocProperties, phDevice);
    if (ZE_RESULT_SUCCESS == res && pMemAllocProperties->pNext) {
        ze_external_memory_export_win32_handle_t *extendedMemoryExportProperties =
            reinterpret_cast<ze_external_memory_export_win32_handle_t *>(pMemAllocProperties->pNext);
        extendedMemoryExportProperties->handle = reinterpret_cast<void *>(reinterpret_cast<uintptr_t *>(driverHandle->mockHandle));
    }

    return res;
}

ze_result_t ContextHandleMock::getImageAllocProperties(Image *image,
                                                       ze_image_allocation_ext_properties_t *pAllocProperties) {

    ze_result_t res = ContextImp::getImageAllocProperties(image, pAllocProperties);
    if (ZE_RESULT_SUCCESS == res && pAllocProperties->pNext) {
        ze_external_memory_export_win32_handle_t *extendedMemoryExportProperties =
            reinterpret_cast<ze_external_memory_export_win32_handle_t *>(pAllocProperties->pNext);
        extendedMemoryExportProperties->handle = reinterpret_cast<void *>(reinterpret_cast<uintptr_t *>(driverHandle->mockHandle));
    }

    return res;
}

ze_result_t ContextHandleMock::freeMem(const void *ptr) {
    L0::ContextImp::freeMem(ptr);
    return ZE_RESULT_SUCCESS;
}

void MemoryExportImportWinHandleTest::SetUp() {

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<DriverHandleGetWinHandleMock>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];

    context = std::make_unique<ContextHandleMock>(driverHandle.get());
    EXPECT_NE(context, nullptr);
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
}

void *DriverHandleGetIpcHandleMock::importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags,
                                                   uint64_t handle, NEO::AllocationType allocationType, void *basePointer, NEO::GraphicsAllocation **pAlloc,
                                                   NEO::SvmAllocationData &mappedPeerAllocData) {
    EXPECT_EQ(handle, static_cast<uint64_t>(mockFd));
    if (mockFd == allocationMap.second) {
        return allocationMap.first;
    }
    return nullptr;
}
ze_result_t ContextGetIpcHandleMock::allocDeviceMem(ze_device_handle_t hDevice,
                                                    const ze_device_mem_alloc_desc_t *deviceDesc,
                                                    size_t size,
                                                    size_t alignment, void **ptr) {
    ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
    if (ZE_RESULT_SUCCESS == res) {
        driverHandle->allocationMap.first = *ptr;
        driverHandle->allocationMap.second = driverHandle->mockFd;
    }

    return res;
}

ze_result_t ContextGetIpcHandleMock::getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) {
    uint64_t handle = driverHandle->mockFd;
    NEO::SvmAllocationData *allocData = this->driverHandle->svmAllocsManager->getSVMAlloc(ptr);

    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(pIpcHandle->data);
    ipcData = {};
    ipcData.handle = handle;
    auto type = Context::parseUSMType(allocData->memoryType);
    if (type == ZE_MEMORY_TYPE_HOST) {
        ipcData.type = static_cast<uint8_t>(InternalIpcMemoryType::hostUnifiedMemory);
    }

    return ZE_RESULT_SUCCESS;
}

NEO::GraphicsAllocation *MemoryManagerOpenIpcMock::allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) {
    return allocateGraphicsMemoryWithProperties(properties, nullptr);
}

NEO::GraphicsAllocation *MemoryManagerOpenIpcMock::allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *externalPtr) {
    auto ptr = reinterpret_cast<void *>(sharedHandleAddress);
    sharedHandleAddress += properties.size;
    auto gmmHelper = getGmmHelper(0);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    auto alloc = new IpcImplicitScalingMockGraphicsAllocation(properties.rootDeviceIndex,
                                                              1u /*num gmms*/,
                                                              NEO::AllocationType::buffer,
                                                              ptr,
                                                              0x1000,
                                                              0u,
                                                              MemoryPool::system4KBPages,
                                                              MemoryManager::maxOsContextCount,
                                                              canonizedGpuAddress);
    alloc->setGpuBaseAddress(0xabcd);
    return alloc;
}

NEO::GraphicsAllocation *MemoryManagerOpenIpcMock::createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) {
    if (failOnCreateGraphicsAllocationFromSharedHandle) {
        return nullptr;
    }
    auto ptr = reinterpret_cast<void *>(sharedHandleAddress);
    sharedHandleAddress += properties.size;
    auto gmmHelper = getGmmHelper(0);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    auto alloc = new IpcImplicitScalingMockGraphicsAllocation(properties.rootDeviceIndex,
                                                              1u /*num gmms*/,
                                                              NEO::AllocationType::buffer,
                                                              ptr,
                                                              0x1000,
                                                              0u,
                                                              MemoryPool::system4KBPages,
                                                              MemoryManager::maxOsContextCount,
                                                              canonizedGpuAddress);
    alloc->setGpuBaseAddress(0xabcd);
    return alloc;
}
NEO::GraphicsAllocation *MemoryManagerOpenIpcMock::createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) {
    if (failOnCreateGraphicsAllocationFromSharedHandle) {
        return nullptr;
    }
    auto ptr = reinterpret_cast<void *>(sharedHandleAddress);
    sharedHandleAddress += properties.size;
    auto gmmHelper = getGmmHelper(0);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    auto alloc = new IpcImplicitScalingMockGraphicsAllocation(properties.rootDeviceIndex,
                                                              1u /*num gmms*/,
                                                              NEO::AllocationType::buffer,
                                                              ptr,
                                                              0x1000,
                                                              0u,
                                                              MemoryPool::system4KBPages,
                                                              MemoryManager::maxOsContextCount,
                                                              canonizedGpuAddress);
    alloc->setGpuBaseAddress(0xabcd);
    return alloc;
}

ze_result_t ContextIpcMock::getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) {
    uint64_t handle = mockFd;
    NEO::SvmAllocationData *allocData = this->driverHandle->svmAllocsManager->getSVMAlloc(ptr);

    IpcMemoryData &ipcData = *reinterpret_cast<IpcMemoryData *>(pIpcHandle->data);
    ipcData = {};
    ipcData.handle = handle;
    auto type = Context::parseUSMType(allocData->memoryType);
    if (type == ZE_MEMORY_TYPE_HOST) {
        ipcData.type = static_cast<uint8_t>(InternalIpcMemoryType::hostUnifiedMemory);
    }

    return ZE_RESULT_SUCCESS;
}

void MemoryOpenIpcHandleTest::SetUp() {

    neoDevice =
        NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<DriverHandleImp>();
    driverHandle->initialize(std::move(devices));
    prevMemoryManager = driverHandle->getMemoryManager();
    currMemoryManager = new MemoryManagerOpenIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(currMemoryManager);
    device = driverHandle->devices[0];

    context = std::make_unique<ContextIpcMock>(driverHandle.get());
    EXPECT_NE(context, nullptr);
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
}

void MemoryOpenIpcHandleTest::TearDown() {
    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;
}

NEO::GraphicsAllocation *MemoryManagerIpcImplicitScalingMock::allocateGraphicsMemoryInPreferredPool(const AllocationProperties &properties, const void *hostPtr) {
    auto ptr = reinterpret_cast<void *>(sharedHandleAddress);
    sharedHandleAddress += properties.size;
    auto gmmHelper = getGmmHelper(0);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    auto alloc = new IpcImplicitScalingMockGraphicsAllocation(0u,
                                                              1u /*num gmms*/,
                                                              NEO::AllocationType::buffer,
                                                              ptr,
                                                              0x1000,
                                                              0u,
                                                              MemoryPool::system4KBPages,
                                                              MemoryManager::maxOsContextCount,
                                                              canonizedGpuAddress);
    alloc->setGpuBaseAddress(0xabcd);
    return alloc;
}

NEO::GraphicsAllocation *MemoryManagerIpcImplicitScalingMock::allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) {
    auto ptr = reinterpret_cast<void *>(sharedHandleAddress);
    sharedHandleAddress += properties.size;
    auto gmmHelper = getGmmHelper(0);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    auto alloc = new IpcImplicitScalingMockGraphicsAllocation(0u,
                                                              1u /*num gmms*/,
                                                              NEO::AllocationType::buffer,
                                                              ptr,
                                                              0x1000,
                                                              0u,
                                                              MemoryPool::system4KBPages,
                                                              MemoryManager::maxOsContextCount,
                                                              canonizedGpuAddress);
    alloc->setGpuBaseAddress(0xabcd);
    return alloc;
}

NEO::GraphicsAllocation *MemoryManagerIpcImplicitScalingMock::createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) {
    if (failOnCreateGraphicsAllocationFromSharedHandle) {
        return nullptr;
    }
    auto ptr = reinterpret_cast<void *>(sharedHandleAddress);
    sharedHandleAddress += properties.size;
    auto gmmHelper = getGmmHelper(0);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    auto alloc = new IpcImplicitScalingMockGraphicsAllocation(0u,
                                                              1u /*num gmms*/,
                                                              NEO::AllocationType::buffer,
                                                              ptr,
                                                              0x1000,
                                                              0u,
                                                              MemoryPool::system4KBPages,
                                                              MemoryManager::maxOsContextCount,
                                                              canonizedGpuAddress);
    alloc->setGpuBaseAddress(0xabcd);
    return alloc;
}

NEO::GraphicsAllocation *MemoryManagerIpcImplicitScalingMock::createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) {
    if (failOnCreateGraphicsAllocationFromSharedHandle) {
        return nullptr;
    }
    auto ptr = reinterpret_cast<void *>(sharedHandleAddress);
    sharedHandleAddress += properties.size;
    auto gmmHelper = getGmmHelper(0);
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
    auto alloc = new IpcImplicitScalingMockGraphicsAllocation(0u,
                                                              1u /*num gmms*/,
                                                              NEO::AllocationType::buffer,
                                                              ptr,
                                                              0x1000,
                                                              0u,
                                                              MemoryPool::system4KBPages,
                                                              MemoryManager::maxOsContextCount,
                                                              canonizedGpuAddress);
    alloc->setGpuBaseAddress(0xabcd);
    return alloc;
}

void MemoryExportImportImplicitScalingTest::SetUp() {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableImplicitScaling.set(1);
    debugManager.flags.EnableWalkerPartition.set(1);

    neoDevice =
        NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<DriverHandleImp>();
    driverHandle->initialize(std::move(devices));
    prevMemoryManager = driverHandle->getMemoryManager();
    currMemoryManager = new MemoryManagerIpcImplicitScalingMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(currMemoryManager);

    prevSvmAllocsManager = driverHandle->svmAllocsManager;
    currSvmAllocsManager = new NEO::SVMAllocsManager(currMemoryManager);
    driverHandle->svmAllocsManager = currSvmAllocsManager;

    device = driverHandle->devices[0];

    context = std::make_unique<ContextIpcMock>(driverHandle.get());
    EXPECT_NE(context, nullptr);
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
}

void MemoryExportImportImplicitScalingTest::TearDown() {
    // cleanup pool before restoring svm manager
    for (auto device : driverHandle->devices) {
        device->getNEODevice()->cleanupUsmAllocationPool();
        device->getNEODevice()->resetUsmAllocationPool(nullptr);
    }
    driverHandle->svmAllocsManager = prevSvmAllocsManager;
    delete currSvmAllocsManager;
    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;
}

void MemoryGetIpcHandlePidfdTest::SetUp() {
    neoDevice =
        NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<DriverHandleImp>();
    driverHandle->initialize(std::move(devices));
    prevMemoryManager = driverHandle->getMemoryManager();
    currMemoryManager = new MemoryManagerOpenIpcMock(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(currMemoryManager);
    device = driverHandle->devices[0];

    context = std::make_unique<ContextImp>(driverHandle.get());
    EXPECT_NE(context, nullptr);
    context->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
    auto neoDevice = device->getNEODevice();
    context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
    context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
}
void MemoryGetIpcHandlePidfdTest::TearDown() {
    driverHandle->setMemoryManager(prevMemoryManager);
    delete currMemoryManager;
}

} // namespace ult
} // namespace L0

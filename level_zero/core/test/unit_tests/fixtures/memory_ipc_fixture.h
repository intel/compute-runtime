/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

struct DriverHandleGetFdMock : public L0::DriverHandleImp {
    void *importFdHandle(ze_device_handle_t hDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::GraphicsAllocation **pAloc) override {
        if (mockFd == allocationMap.second) {
            return allocationMap.first;
        }
        return nullptr;
    }

    const int mockFd = 57;
    std::pair<void *, int> allocationMap;
};

struct ContextFdMock : public L0::ContextImp {
    ContextFdMock(DriverHandleGetFdMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override {
        ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
        if (ZE_RESULT_SUCCESS == res) {
            driverHandle->allocationMap.first = *ptr;
            driverHandle->allocationMap.second = driverHandle->mockFd;
        }

        return res;
    }

    ze_result_t getMemAllocProperties(const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) override {
        ze_result_t res = ContextImp::getMemAllocProperties(ptr, pMemAllocProperties, phDevice);
        if (ZE_RESULT_SUCCESS == res && pMemAllocProperties->pNext) {
            ze_external_memory_export_fd_t *extendedMemoryExportProperties =
                reinterpret_cast<ze_external_memory_export_fd_t *>(pMemAllocProperties->pNext);
            extendedMemoryExportProperties->fd = driverHandle->mockFd;
        }

        return res;
    }

    ze_result_t closeIpcMemHandle(const void *ptr) override {
        return ZE_RESULT_SUCCESS;
    }

    DriverHandleGetFdMock *driverHandle = nullptr;
};

struct MemoryExportImportTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleGetFdMock>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        context = std::make_unique<ContextFdMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
    }
    std::unique_ptr<DriverHandleGetFdMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    ze_context_handle_t hContext;
    std::unique_ptr<ContextFdMock> context;
};

struct DriverHandleGetMemHandleMock : public L0::DriverHandleImp {
    void *importNTHandle(ze_device_handle_t hDevice, void *handle) override {
        if (mockHandle == allocationHandleMap.second) {
            return allocationHandleMap.first;
        }
        return nullptr;
    }
    void *importFdHandle(ze_device_handle_t hDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::GraphicsAllocation **pAloc) override {
        if (mockFd == allocationFdMap.second) {
            return allocationFdMap.first;
        }
        return nullptr;
    }

    const int mockFd = 57;
    std::pair<void *, int> allocationFdMap;
    uint64_t mockHandle = 57;
    std::pair<void *, uint64_t> allocationHandleMap;
};

struct ContextMemHandleMock : public L0::ContextImp {
    ContextMemHandleMock(DriverHandleGetMemHandleMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override {
        ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
        if (ZE_RESULT_SUCCESS == res) {
            driverHandle->allocationFdMap.first = *ptr;
            driverHandle->allocationFdMap.second = driverHandle->mockFd;
            driverHandle->allocationHandleMap.first = *ptr;
            driverHandle->allocationHandleMap.second = driverHandle->mockHandle;
        }

        return res;
    }

    ze_result_t getMemAllocProperties(const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) override {
        ze_result_t res = ContextImp::getMemAllocProperties(ptr, pMemAllocProperties, phDevice);
        if (ZE_RESULT_SUCCESS == res && pMemAllocProperties->pNext) {
            ze_external_memory_export_fd_t *extendedMemoryExportProperties =
                reinterpret_cast<ze_external_memory_export_fd_t *>(pMemAllocProperties->pNext);
            extendedMemoryExportProperties->fd = driverHandle->mockFd;
        }

        return res;
    }

    ze_result_t closeIpcMemHandle(const void *ptr) override {
        return ZE_RESULT_SUCCESS;
    }

    DriverHandleGetMemHandleMock *driverHandle = nullptr;
};

struct MemoryExportImportWSLTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
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
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }
    std::unique_ptr<DriverHandleGetMemHandleMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    ze_context_handle_t hContext;
    std::unique_ptr<ContextMemHandleMock> context;
    NEO::MemoryManager *prevMemoryManager = nullptr;
    MemoryManagerMemHandleMock *currMemoryManager = nullptr;
};

struct DriverHandleGetWinHandleMock : public L0::DriverHandleImp {
    void *importNTHandle(ze_device_handle_t hDevice, void *handle) override {
        if (mockHandle == allocationMap.second) {
            return allocationMap.first;
        }
        return nullptr;
    }

    uint64_t mockHandle = 57;
    std::pair<void *, uint64_t> allocationMap;
};

struct ContextHandleMock : public L0::ContextImp {
    ContextHandleMock(DriverHandleGetWinHandleMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override {
        ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
        if (ZE_RESULT_SUCCESS == res) {
            driverHandle->allocationMap.first = *ptr;
            driverHandle->allocationMap.second = driverHandle->mockHandle;
        }

        return res;
    }

    ze_result_t getMemAllocProperties(const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) override {
        ze_result_t res = ContextImp::getMemAllocProperties(ptr, pMemAllocProperties, phDevice);
        if (ZE_RESULT_SUCCESS == res && pMemAllocProperties->pNext) {
            _ze_external_memory_export_win32_handle_t *extendedMemoryExportProperties =
                reinterpret_cast<_ze_external_memory_export_win32_handle_t *>(pMemAllocProperties->pNext);
            extendedMemoryExportProperties->handle = reinterpret_cast<void *>(reinterpret_cast<uintptr_t *>(driverHandle->mockHandle));
        }

        return res;
    }
    ze_result_t freeMem(const void *ptr) override {
        L0::ContextImp::freeMem(ptr);
        return ZE_RESULT_SUCCESS;
    }

    DriverHandleGetWinHandleMock *driverHandle = nullptr;
};

struct MemoryExportImportWinHandleTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<DriverHandleGetWinHandleMock>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        context = std::make_unique<ContextHandleMock>(driverHandle.get());
        EXPECT_NE(context, nullptr);
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
    }
    std::unique_ptr<DriverHandleGetWinHandleMock> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    ze_context_handle_t hContext;
    std::unique_ptr<ContextHandleMock> context;
};

struct DriverHandleGetIpcHandleMock : public DriverHandleImp {
    void *importFdHandle(ze_device_handle_t hDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::GraphicsAllocation **pAlloc) override {
        EXPECT_EQ(handle, static_cast<uint64_t>(mockFd));
        if (mockFd == allocationMap.second) {
            return allocationMap.first;
        }
        return nullptr;
    }

    const int mockFd = 999;
    std::pair<void *, int> allocationMap;
};

struct ContextGetIpcHandleMock : public L0::ContextImp {
    ContextGetIpcHandleMock(DriverHandleGetIpcHandleMock *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }
    ze_result_t allocDeviceMem(ze_device_handle_t hDevice,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment, void **ptr) override {
        ze_result_t res = L0::ContextImp::allocDeviceMem(hDevice, deviceDesc, size, alignment, ptr);
        if (ZE_RESULT_SUCCESS == res) {
            driverHandle->allocationMap.first = *ptr;
            driverHandle->allocationMap.second = driverHandle->mockFd;
        }

        return res;
    }

    ze_result_t getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) override {
        uint64_t handle = driverHandle->mockFd;
        memcpy_s(reinterpret_cast<void *>(pIpcHandle->data),
                 sizeof(ze_ipc_mem_handle_t),
                 &handle,
                 sizeof(handle));

        return ZE_RESULT_SUCCESS;
    }

    DriverHandleGetIpcHandleMock *driverHandle = nullptr;
};

class MemoryManagerIpcMock : public NEO::MemoryManager {
  public:
    MemoryManagerIpcMock(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MemoryManager(executionEnvironment) {}
    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override { return nullptr; }
    void addAllocationToHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    void removeAllocationFromHostPtrManager(NEO::GraphicsAllocation *memory) override{};
    NEO::GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override { return nullptr; };
    AllocationStatus populateOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override { return AllocationStatus::Success; };
    void cleanOsHandles(NEO::OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};
    void freeGraphicsMemoryImpl(NEO::GraphicsAllocation *gfxAllocation) override{};
    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override{};
    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override { return 0; };
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override { return 0; };
    double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) override { return 0; }
    AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) override {
        return {};
    }
    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override{};
    NEO::GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateUSMHostGraphicsMemory(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemory64kb(const NEO::AllocationData &allocationData) override { return nullptr; };
    NEO::GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const NEO::AllocationData &allocationData, bool useLocalMemory) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const NEO::AllocationData &allocationData, AllocationStatus &status) override { return nullptr; };
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const NEO::AllocationData &allocationData) override { return nullptr; };

    NEO::GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const NEO::AllocationData &allocationData, std::unique_ptr<Gmm> gmm) override { return nullptr; };
    NEO::GraphicsAllocation *allocateMemoryByKMD(const NEO::AllocationData &allocationData) override { return nullptr; };
    void *lockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override { return nullptr; };
    void unlockResourceImpl(NEO::GraphicsAllocation &graphicsAllocation) override{};
};

class MemoryManagerOpenIpcMock : public MemoryManagerIpcMock {
  public:
    MemoryManagerOpenIpcMock(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerIpcMock(executionEnvironment) {}
    NEO::GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override {
        if (failOnCreateGraphicsAllocationFromSharedHandle) {
            return nullptr;
        }
        auto alloc = new NEO::MockGraphicsAllocation(0,
                                                     NEO::AllocationType::BUFFER,
                                                     reinterpret_cast<void *>(sharedHandleAddress++),
                                                     0x1000,
                                                     0,
                                                     sizeof(uint32_t),
                                                     MemoryPool::System4KBPages);
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    }
    NEO::GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override {
        auto alloc = new NEO::MockGraphicsAllocation(0,
                                                     NEO::AllocationType::BUFFER,
                                                     reinterpret_cast<void *>(sharedHandleAddress++),
                                                     0x1000,
                                                     0,
                                                     sizeof(uint32_t),
                                                     MemoryPool::System4KBPages);
        alloc->setGpuBaseAddress(0xabcd);
        return alloc;
    };

    uint64_t sharedHandleAddress = 0x1234;

    bool failOnCreateGraphicsAllocationFromSharedHandle = false;
};

struct ContextIpcMock : public L0::ContextImp {
    ContextIpcMock(DriverHandleImp *inDriverHandle) : L0::ContextImp(static_cast<L0::DriverHandle *>(inDriverHandle)) {
        driverHandle = inDriverHandle;
    }

    ze_result_t getIpcMemHandle(const void *ptr, ze_ipc_mem_handle_t *pIpcHandle) override {
        uint64_t handle = mockFd;
        memcpy_s(reinterpret_cast<void *>(pIpcHandle->data),
                 sizeof(ze_ipc_mem_handle_t),
                 &handle,
                 sizeof(handle));

        return ZE_RESULT_SUCCESS;
    }

    const int mockFd = 999;
};

struct MemoryOpenIpcHandleTest : public ::testing::Test {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        neoDevice =
            NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        auto mockBuiltIns = new MockBuiltins();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
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
        context->getDevices().insert(std::make_pair(device->toHandle(), device));
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        driverHandle->setMemoryManager(prevMemoryManager);
        delete currMemoryManager;
    }
    NEO::MemoryManager *prevMemoryManager = nullptr;
    NEO::MemoryManager *currMemoryManager = nullptr;
    std::unique_ptr<DriverHandleImp> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    std::unique_ptr<ContextIpcMock> context;
};

} // namespace ult
} // namespace L0
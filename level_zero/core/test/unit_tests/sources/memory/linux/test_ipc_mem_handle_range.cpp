/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

#include <cstring>
#include <limits>
#include <set>
#include <vector>

namespace L0 {
namespace ult {

namespace {
constexpr uint32_t baseHandleValue = 100u;
constexpr int transportHandleFd = 200;
uint8_t gRangeTransportStorage[4096] = {};
std::set<int> gClosedImportFds;
} // namespace

class RangeTransportSvmManager : public NEO::MockSVMAllocsManager {
  public:
    RangeTransportSvmManager(NEO::MemoryManager *memoryManager) : NEO::MockSVMAllocsManager(memoryManager) {}

    void *createHostUnifiedMemoryAllocation(size_t size, const NEO::UnifiedMemoryProperties &memoryProperties) override {
        createHostUnifiedCalled = true;
        lastRequestedSize = size;
        if (failCreateHostUnified) {
            return nullptr;
        }
        if (returnUntrackedTransport) {
            return gRangeTransportStorage;
        }
        auto alloc = new NEO::MockGraphicsAllocation(0u, gRangeTransportStorage, size);
        NEO::SvmAllocationData allocData(0u);
        allocData.gpuAllocations.addAllocation(alloc);
        allocData.cpuAllocation = nullptr;
        allocData.size = size;
        allocData.memoryType = InternalMemoryType::hostUnifiedMemory;
        allocData.device = nullptr;
        allocData.setAllocId(++this->allocationsCounter);
        this->insertSVMAlloc(gRangeTransportStorage, allocData);
        return gRangeTransportStorage;
    }

    bool createHostUnifiedCalled = false;
    bool failCreateHostUnified = false;
    bool returnUntrackedTransport = false;
    size_t lastRequestedSize = 0u;
};

class RecordingRangeMemoryManager : public MemoryManagerOpenIpcMock {
  public:
    RecordingRangeMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerOpenIpcMock(executionEnvironment) {}

    NEO::GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<NEO::osHandle> &handles,
                                                                               NEO::AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        recordedHandles = handles;
        return MemoryManagerOpenIpcMock::createGraphicsAllocationFromMultipleSharedHandles(handles, properties, requireSpecificBitness, isHostIpcAllocation, reuseSharedAllocation, mapPointer);
    }

    std::vector<NEO::osHandle> recordedHandles;
};

class NullRangeAllocMemoryManager : public MemoryManagerOpenIpcMock {
  public:
    NullRangeAllocMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerOpenIpcMock(executionEnvironment) {}

    void *importFdHandles(NEO::Device *neoDevice, NEO::SVMAllocsManager *svmAllocsManager, const std::vector<NEO::osHandle> &handles, void *basePointer, NEO::GraphicsAllocation **pAlloc, NEO::SvmAllocationData &mappedPeerAllocData, bool compressedMemory, bool uncachedBias, const std::vector<uint64_t> &physicalOffsets) override {
        importFdHandlesCalled = true;
        lastPhysicalOffsets = physicalOffsets;
        lastUncachedBias = uncachedBias;
        if (pAlloc != nullptr) {
            *pAlloc = nullptr;
        }
        return &returnedPtrStorage;
    }

    bool importFdHandlesCalled = false;
    bool lastUncachedBias = false;
    std::vector<uint64_t> lastPhysicalOffsets;
    uint64_t returnedPtrStorage = 0u;
};

class HostRangeAllocMemoryManager : public MemoryManagerOpenIpcMock {
  public:
    HostRangeAllocMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerOpenIpcMock(executionEnvironment) {}

    NEO::GraphicsAllocation *createHostAllocationFromMultipleSharedHandles(const std::vector<NEO::osHandle> &handles, NEO::AllocationProperties &properties, const std::vector<uint64_t> &physicalOffsets, bool reuseSharedAllocation) override {
        createHostAllocationCalled = true;
        lastPhysicalOffsets = physicalOffsets;
        return new NEO::MockGraphicsAllocation(properties.rootDeviceIndex, reinterpret_cast<void *>(0x9000), MemoryConstants::pageSize);
    }

    bool createHostAllocationCalled = false;
    std::vector<uint64_t> lastPhysicalOffsets;
};

class MockRangeFailingGraphicsAllocation : public NEO::MockGraphicsAllocation {
  public:
    MockRangeFailingGraphicsAllocation(uint32_t rootDeviceIndex, void *buffer, size_t sizeIn)
        : NEO::MockGraphicsAllocation(rootDeviceIndex, buffer, sizeIn) {}

    int createInternalHandle(NEO::MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle, void *reservedHandleData) override {
        return -1;
    }
};

class ReservedDataRecordingAllocation : public NEO::MockGraphicsAllocation {
  public:
    ReservedDataRecordingAllocation(uint32_t rootDeviceIndex, void *buffer, size_t sizeIn)
        : NEO::MockGraphicsAllocation(rootDeviceIndex, buffer, sizeIn) {}

    int createInternalHandle(NEO::MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle, void *reservedHandleData) override {
        reservedHandleDataWasSet = reservedHandleData != nullptr;
        handle = 0u;
        return 0;
    }

    bool reservedHandleDataWasSet = false;
};

struct RangeExportContext : public L0::Context {
    RangeExportContext(L0::DriverHandle *inDriverHandle) : L0::Context(inDriverHandle) {
        driverHandle = inDriverHandle;
        settings.enableIpcHandleSharingByDefault = true;
        settings.handleType = IpcHandleType::fdHandle;
    }

    void setUseOpaqueHandle(uint8_t value) { settings.useOpaqueHandle = value; }
    size_t rangeTransportCount() { return ipcRangeTransports.size(); }

    void injectRangeTransport(const void *baseAddress, const void *transportPtr, uint64_t transportHandleKey, const std::vector<uint64_t> &handleKeys) {
        IpcRangeTransportEntry entry;
        entry.baseAddress = baseAddress;
        entry.transportPtr = transportPtr;
        entry.transportHandleKey = transportHandleKey;
        entry.handleKeys = handleKeys;
        ipcRangeTransports.push_back(std::move(entry));
        ipcRangeTransportsPresent.store(true, std::memory_order_release);
    }

    ze_result_t callGetIpcRangeHandle(const void *ptr, const ze_ipc_phys_mem_handle_range_ext_desc_t *desc, ze_ipc_mem_handle_t *pIpcHandle) {
        return getIpcRangeHandle(ptr, desc, pIpcHandle);
    }
    ze_result_t callEncodeIpcHandleForRangeAllocation(NEO::GraphicsAllocation *alloc, uint64_t ptrAddress, ze_ipc_mem_handle_t &ipcHandle,
                                                      uint8_t ipcType = static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory),
                                                      uint64_t physicalOffset = 0u,
                                                      void *reservedHandleData = nullptr) {
        return encodeIpcHandleForRangeAllocation(alloc, ptrAddress, ipcType, physicalOffset, reservedHandleData, ipcHandle);
    }
    ze_result_t callOpenIpcRangeHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t &ipcHandle, ze_ipc_memory_flags_t flags, void **pptr) {
        return openIpcRangeHandle(hDevice, ipcHandle, flags, pptr);
    }
    ze_result_t callPutIpcRangeHandle(const ze_ipc_mem_handle_t &ipcHandle) { return putIpcRangeHandle(ipcHandle); }
    bool callIsIpcRangeHandle(const ze_ipc_mem_handle_t &ipcHandle) const { return isIpcRangeHandle(ipcHandle); }
    bool callReleaseIpcRangeTransportForPtr(const void *ptr) { return releaseIpcRangeTransportForPtr(ptr); }
    uint32_t getMaxIpcRangeHandleCount() override { return maxIpcRangeHandleCount; }

    uint32_t maxIpcRangeHandleCount = 1024u;
};

struct RangeImportContext : public L0::Context {
    RangeImportContext(L0::DriverHandle *inDriverHandle) : L0::Context(inDriverHandle) {
        driverHandle = inDriverHandle;
        settings.enableIpcHandleSharingByDefault = true;
        settings.handleType = IpcHandleType::fdHandle;
    }

    void setUseOpaqueHandle(uint8_t value) { settings.useOpaqueHandle = value; }
    ze_result_t callOpenIpcRangeHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t &ipcHandle, ze_ipc_memory_flags_t flags, void **pptr) {
        return openIpcRangeHandle(hDevice, ipcHandle, flags, pptr);
    }

    std::pair<NEO::GraphicsAllocation *, void *> getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, bool isHostIpcAllocation, unsigned int processId, ze_ipc_memory_flags_t flags, uint64_t cacheID, void *reservedHandleData, bool compressedMemory, bool isOpaqueHandle, uint64_t physicalOffset) override {
        getMemHandlePtrCalled = true;
        if (transportImportFails) {
            return {nullptr, nullptr};
        }
        if (transportImportWithoutAllocation) {
            return {nullptr, gRangeTransportStorage};
        }
        transportAllocation = std::make_unique<NEO::MockGraphicsAllocation>(0u, gRangeTransportStorage, transportAllocationSize);
        return {transportAllocation.get(), gRangeTransportStorage};
    }

    uint32_t getMaxIpcRangeHandleCount() override { return maxIpcRangeHandleCount; }

    bool getMemHandlePtrCalled = false;
    bool transportImportFails = false;
    bool transportImportWithoutAllocation = false;
    size_t transportAllocationSize = sizeof(gRangeTransportStorage);
    std::unique_ptr<NEO::MockGraphicsAllocation> transportAllocation;
    uint32_t maxIpcRangeHandleCount = 1024u;
};

struct IpcMemHandleRangeTest : public MemoryOpenIpcHandleTest {
    void SetUp() override {
        MemoryOpenIpcHandleTest::SetUp();
        std::memset(gRangeTransportStorage, 0, sizeof(gRangeTransportStorage));
        gClosedImportFds.clear();
    }

    void TearDown() override {
        clearReservation();
        drainIpcHandleMap();
        gClosedImportFds.clear();
        if (rangeSvmManager) {
            driverHandle->svmAllocsManager = previousSvmManager;
        }
        MemoryOpenIpcHandleTest::TearDown();
        rangeSvmManager.reset();
    }

    void drainIpcHandleMap() {
        auto lock = driverHandle->lockIPCHandleMap();
        auto &ipcMap = driverHandle->getIPCHandleMap();
        for (auto &entry : ipcMap) {
            delete entry.second;
        }
        ipcMap.clear();
    }

    void installExportSvmManager() {
        previousSvmManager = driverHandle->svmAllocsManager;
        rangeSvmManager = std::make_unique<RangeTransportSvmManager>(driverHandle->getMemoryManager());
        driverHandle->svmAllocsManager = rangeSvmManager.get();
    }

    std::unique_ptr<RangeExportContext> createExportContext() {
        auto exportContext = std::make_unique<RangeExportContext>(driverHandle.get());
        exportContext->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        exportContext->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        exportContext->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        return exportContext;
    }

    std::unique_ptr<RangeImportContext> createImportContext() {
        auto importContext = std::make_unique<RangeImportContext>(driverHandle.get());
        importContext->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        importContext->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        importContext->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
        return importContext;
    }

    void buildRangeTransport(uint32_t numHandles, bool matchingOpaqueHandle = true, uint8_t memoryType = static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory), uint64_t perChunkPoolOffset = 0u, uint64_t leadingOffset = 0u, bool rangeIsHost = false) {
        IpcRangeTransportHeader header{};
        header.magic = ipcRangeHandleMagic;
        header.version = ipcRangeTransportVersion;
        header.numHandles = numHandles;
        header.leadingOffset = leadingOffset;
        header.rangeIsHost = rangeIsHost ? 1u : 0u;
        std::memcpy(gRangeTransportStorage, &header, sizeof(header));

        auto handlesBase = reinterpret_cast<ze_ipc_mem_handle_t *>(gRangeTransportStorage + sizeof(header));
        for (uint32_t i = 0; i < numHandles; i++) {
            ze_ipc_mem_handle_t entry = {};
            IpcOpaqueMemoryData opaque{};
            opaque.handle.fd = static_cast<int>(baseHandleValue + i);
            opaque.opaqueHandle.fd = matchingOpaqueHandle ? static_cast<int>(baseHandleValue + i) : static_cast<int>(baseHandleValue + i + 1000u);
            opaque.type = IpcHandleType::fdHandle;
            opaque.memoryType = memoryType;
            opaque.poolOffset = perChunkPoolOffset;
            std::memcpy(entry.data, &opaque, sizeof(opaque));
            handlesBase[i] = entry;
        }
    }

    ze_ipc_mem_handle_t buildRangeHandle(int transportFd = transportHandleFd) {
        ze_ipc_mem_handle_t handle = {};
        IpcOpaqueMemoryData rangeData{};
        rangeData.handle.fd = transportFd;
        rangeData.opaqueHandle.fd = transportFd;
        rangeData.type = IpcHandleType::fdHandle;
        rangeData.memoryType = static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport);
        std::memcpy(handle.data, &rangeData, sizeof(rangeData));
        return handle;
    }

    ze_ipc_mem_handle_t buildOpaqueHandle(int fd, uint8_t memoryType) {
        ze_ipc_mem_handle_t handle = {};
        IpcOpaqueMemoryData opaque{};
        opaque.handle.fd = fd;
        opaque.opaqueHandle.fd = fd;
        opaque.type = IpcHandleType::fdHandle;
        opaque.memoryType = memoryType;
        std::memcpy(handle.data, &opaque, sizeof(opaque));
        return handle;
    }

    ze_ipc_phys_mem_handle_range_ext_desc_t buildRangeDesc(size_t size) {
        ze_ipc_phys_mem_handle_range_ext_desc_t desc = {};
        desc.stype = ZE_STRUCTURE_TYPE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_DESC;
        desc.pNext = nullptr;
        desc.size = size;
        return desc;
    }

    void installReservation(void *basePtr, const std::vector<NEO::MemoryMappedRange *> &ranges) {
        reservation = std::make_unique<NEO::VirtualMemoryReservation>();
        reservation->rootDeviceIndex = neoDevice->getRootDeviceIndex();
        for (auto *range : ranges) {
            reservation->mappedAllocations[const_cast<void *>(range->ptr)] = range;
        }
        auto memoryManager = driverHandle->getMemoryManager();
        auto lock = memoryManager->lockVirtualMemoryReservationMap();
        memoryManager->getVirtualMemoryReservationMap()[basePtr] = reservation.get();
        installedReservationBase = basePtr;
    }

    void clearReservation() {
        if (installedReservationBase != nullptr) {
            auto memoryManager = driverHandle->getMemoryManager();
            auto lock = memoryManager->lockVirtualMemoryReservationMap();
            memoryManager->getVirtualMemoryReservationMap().erase(installedReservationBase);
            installedReservationBase = nullptr;
        }
        reservation.reset();
    }

    void enableChunkImportMocks() {
        NEO::SysCalls::sysCallsPidfdOpen = [](pid_t, unsigned int) -> int { return 77; };
        NEO::SysCalls::sysCallsPidfdGetfd = [](int, int fd, unsigned int) -> int { return fd; };
    }

    std::unique_ptr<NEO::VirtualMemoryReservation> reservation;
    void *installedReservationBase = nullptr;
    std::unique_ptr<RangeTransportSvmManager> rangeSvmManager;
    NEO::SVMAllocsManager *previousSvmManager = nullptr;

    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdOpen)> pidfdOpenBackup{&NEO::SysCalls::sysCallsPidfdOpen};
    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdGetfd)> pidfdGetfdBackup{&NEO::SysCalls::sysCallsPidfdGetfd};
};

TEST_F(IpcMemHandleRangeTest, givenRangeDescWithZeroSizeWhenGettingIpcHandleWithPropertiesThenInvalidSizeIsReturned) {
    auto exportContext = createExportContext();
    uint64_t dummy = 0;
    ze_ipc_mem_handle_t ipcHandle = {};
    auto desc = buildRangeDesc(0u);
    auto result = exportContext->getIpcMemHandle(&dummy, &desc, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);
}

TEST_F(IpcMemHandleRangeTest, givenRangeDescAndPtrNotInReservationMapWhenGettingIpcHandleWithPropertiesThenInvalidArgumentIsReturned) {
    auto exportContext = createExportContext();
    uint64_t dummy = 0;
    ze_ipc_mem_handle_t ipcHandle = {};
    auto desc = buildRangeDesc(4096u);
    auto result = exportContext->getIpcMemHandle(&dummy, &desc, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(IpcMemHandleRangeTest, givenNullDescWhenGettingRangeHandleThenInvalidArgumentIsReturned) {
    auto exportContext = createExportContext();
    void *basePtr = reinterpret_cast<void *>(0x500000);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->callGetIpcRangeHandle(basePtr, nullptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(IpcMemHandleRangeTest, givenZeroSizedDescWhenCallingGetIpcRangeHandleDirectlyThenInvalidSizeIsReturned) {
    auto exportContext = createExportContext();
    void *basePtr = reinterpret_cast<void *>(0x501000);
    auto desc = buildRangeDesc(0u);
    ze_ipc_mem_handle_t ipcHandle = {};

    auto result = exportContext->callGetIpcRangeHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);
}

TEST_F(IpcMemHandleRangeTest, givenNullPtrWhenGettingRangeHandleThenInvalidNullPointerIsReturned) {
    auto exportContext = createExportContext();
    auto desc = buildRangeDesc(4096u);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(nullptr, &desc, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, result);
}

TEST_F(IpcMemHandleRangeTest, givenNullIpcHandlePointerWhenGettingRangeHandleThenInvalidArgumentIsReturned) {
    auto exportContext = createExportContext();
    void *basePtr = reinterpret_cast<void *>(0x600000);
    auto desc = buildRangeDesc(4096u);
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(IpcMemHandleRangeTest, givenContiguousReservationMappingsWhenGettingRangeHandleThenRangeHandleAndTransportAreCreated) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0x100000);
    const size_t mappingSize = 4096u;
    const uint32_t numMappings = 2u;
    void *secondPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(basePtr) + mappingSize);

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    NEO::MockGraphicsAllocation alloc1(neoDevice->getRootDeviceIndex(), secondPtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    NEO::MemoryMappedRange range1{};
    range1.ptr = secondPtr;
    range1.size = mappingSize;
    range1.mappedAllocation.allocation = &alloc1;

    installReservation(basePtr, {&range0, &range1});

    auto desc = buildRangeDesc(numMappings * mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(exportContext->callIsIpcRangeHandle(ipcHandle));
    EXPECT_EQ(1u, exportContext->rangeTransportCount());
    EXPECT_TRUE(rangeSvmManager->createHostUnifiedCalled);

    auto header = reinterpret_cast<const IpcRangeTransportHeader *>(gRangeTransportStorage);
    EXPECT_EQ(ipcRangeHandleMagic, header->magic);
    EXPECT_EQ(ipcRangeTransportVersion, header->version);
    EXPECT_EQ(numMappings, header->numHandles);

    EXPECT_EQ(ZE_RESULT_SUCCESS, exportContext->putIpcMemHandle(ipcHandle));
    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenPtrOffsetIntoReservationWithContiguousMappingsWhenGettingRangeHandleThenRangeHandleAndTransportAreCreated) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *reservationBase = reinterpret_cast<void *>(0x400000);
    const size_t mappingSize = 4096u;
    void *firstPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(reservationBase) + mappingSize);
    void *secondPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(firstPtr) + mappingSize);

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), firstPtr, mappingSize);
    NEO::MockGraphicsAllocation alloc1(neoDevice->getRootDeviceIndex(), secondPtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = firstPtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    NEO::MemoryMappedRange range1{};
    range1.ptr = secondPtr;
    range1.size = mappingSize;
    range1.mappedAllocation.allocation = &alloc1;

    installReservation(reservationBase, {&range0, &range1});
    reservation->virtualAddressRange.address = reinterpret_cast<uint64_t>(reservationBase);
    reservation->virtualAddressRange.size = 0x10000u;

    auto desc = buildRangeDesc(2u * mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(firstPtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(exportContext->callIsIpcRangeHandle(ipcHandle));
    EXPECT_EQ(1u, exportContext->rangeTransportCount());

    EXPECT_EQ(ZE_RESULT_SUCCESS, exportContext->putIpcMemHandle(ipcHandle));
    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenPtrInteriorToMappingWhenGettingRangeHandleThenLeadingOffsetIsRecordedInTransportHeader) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *reservationBase = reinterpret_cast<void *>(0x400000);
    const size_t mappingSize = 0x2000u;
    const uint64_t interiorDelta = 0x800u;
    void *interiorPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(reservationBase) + interiorDelta);

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), reservationBase, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = reservationBase;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(reservationBase, {&range0});
    reservation->virtualAddressRange.address = reinterpret_cast<uint64_t>(reservationBase);
    reservation->virtualAddressRange.size = 0x10000u;

    auto desc = buildRangeDesc(0x1000u);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(interiorPtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(exportContext->callIsIpcRangeHandle(ipcHandle));
    ASSERT_EQ(1u, exportContext->rangeTransportCount());

    auto header = reinterpret_cast<const IpcRangeTransportHeader *>(gRangeTransportStorage);
    EXPECT_EQ(ipcRangeHandleMagic, header->magic);
    EXPECT_EQ(interiorDelta, header->leadingOffset);

    EXPECT_EQ(ZE_RESULT_SUCCESS, exportContext->putIpcMemHandle(ipcHandle));
    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenNonReservedAllocationWithRangeSizeEqualToAllocationWhenGettingRangeHandleThenFallsBackToSingleHandle) {
    auto exportContext = createExportContext();

    void *ptr = reinterpret_cast<void *>(0x500000);
    const size_t size = 8192u;

    NEO::MockGraphicsAllocation alloc(neoDevice->getRootDeviceIndex(), ptr, size);

    auto mockSvmManager = std::make_unique<NEO::MockSVMAllocsManager>(driverHandle->getMemoryManager());
    auto *previous = driverHandle->svmAllocsManager;
    driverHandle->svmAllocsManager = mockSvmManager.get();

    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
    allocData.size = size;
    allocData.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData.device = neoDevice;
    allocData.gpuAllocations.addAllocation(&alloc);
    mockSvmManager->insertSVMAlloc(ptr, allocData);

    auto desc = buildRangeDesc(size);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(ptr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(exportContext->callIsIpcRangeHandle(ipcHandle));
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    mockSvmManager->removeSVMAlloc(*mockSvmManager->getSVMAlloc(ptr));
    driverHandle->svmAllocsManager = previous;
}

TEST_F(IpcMemHandleRangeTest, givenNonReservedAllocationWithRangeSizeWithinAllocationWhenGettingRangeHandleThenFallsBackToSingleHandle) {
    auto exportContext = createExportContext();

    void *ptr = reinterpret_cast<void *>(0x550000);
    const size_t size = 8192u;

    NEO::MockGraphicsAllocation alloc(neoDevice->getRootDeviceIndex(), ptr, size);

    auto mockSvmManager = std::make_unique<NEO::MockSVMAllocsManager>(driverHandle->getMemoryManager());
    auto *previous = driverHandle->svmAllocsManager;
    driverHandle->svmAllocsManager = mockSvmManager.get();

    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
    allocData.size = size;
    allocData.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData.device = neoDevice;
    allocData.gpuAllocations.addAllocation(&alloc);
    mockSvmManager->insertSVMAlloc(ptr, allocData);

    auto desc = buildRangeDesc(size / 2u);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(ptr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(exportContext->callIsIpcRangeHandle(ipcHandle));
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    mockSvmManager->removeSVMAlloc(*mockSvmManager->getSVMAlloc(ptr));
    driverHandle->svmAllocsManager = previous;
}

TEST_F(IpcMemHandleRangeTest, givenNonReservedAllocationWithRangeSizeExceedingAllocationWhenGettingRangeHandleThenInvalidArgumentIsReturned) {
    auto exportContext = createExportContext();

    void *ptr = reinterpret_cast<void *>(0x600000);
    const size_t size = 4096u;

    NEO::MockGraphicsAllocation alloc(neoDevice->getRootDeviceIndex(), ptr, size);

    auto mockSvmManager = std::make_unique<NEO::MockSVMAllocsManager>(driverHandle->getMemoryManager());
    auto *previous = driverHandle->svmAllocsManager;
    driverHandle->svmAllocsManager = mockSvmManager.get();

    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
    allocData.size = size;
    allocData.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData.device = neoDevice;
    allocData.gpuAllocations.addAllocation(&alloc);
    mockSvmManager->insertSVMAlloc(ptr, allocData);

    auto desc = buildRangeDesc(2u * size);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(ptr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    mockSvmManager->removeSVMAlloc(*mockSvmManager->getSVMAlloc(ptr));
    driverHandle->svmAllocsManager = previous;
}

TEST_F(IpcMemHandleRangeTest, givenNonOpaqueSettingsWhenGettingRangeHandleThenUnsupportedFeatureIsReturned) {
    auto exportContext = createExportContext();
    exportContext->setUseOpaqueHandle(0u);

    void *basePtr = reinterpret_cast<void *>(0x200000);
    const size_t mappingSize = 4096u;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenFailingTransportAllocationWhenGettingRangeHandleThenOutOfHostMemoryIsReturned) {
    installExportSvmManager();
    rangeSvmManager->failCreateHostUnified = true;
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0x300000);
    const size_t mappingSize = 4096u;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenChunkCountExceedingMaxHandleCountWhenGettingRangeHandleThenUnsupportedSizeIsReturned) {
    auto exportContext = createExportContext();
    exportContext->maxIpcRangeHandleCount = 1u;

    void *basePtr = reinterpret_cast<void *>(0x300000);
    const size_t mappingSize = 4096u;
    void *secondPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(basePtr) + mappingSize);

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    NEO::MockGraphicsAllocation alloc1(neoDevice->getRootDeviceIndex(), secondPtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    NEO::MemoryMappedRange range1{};
    range1.ptr = secondPtr;
    range1.size = mappingSize;
    range1.mappedAllocation.allocation = &alloc1;

    installReservation(basePtr, {&range0, &range1});

    auto desc = buildRangeDesc(2u * mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenZeroMaxHandleCountWhenGettingRangeHandleThenUnsupportedFeatureIsReturned) {
    auto exportContext = createExportContext();
    exportContext->maxIpcRangeHandleCount = 0u;

    void *basePtr = reinterpret_cast<void *>(0x300000);
    const size_t mappingSize = 4096u;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;
    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenChunkFailingInternalHandleWhenGettingRangeHandleThenOutOfHostMemoryIsReturnedAndNoTransportIsCreated) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0xA00000);
    const size_t mappingSize = 4096u;
    void *secondPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(basePtr) + mappingSize);

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    MockRangeFailingGraphicsAllocation alloc1(neoDevice->getRootDeviceIndex(), secondPtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    NEO::MemoryMappedRange range1{};
    range1.ptr = secondPtr;
    range1.size = mappingSize;
    range1.mappedAllocation.allocation = &alloc1;

    installReservation(basePtr, {&range0, &range1});

    auto desc = buildRangeDesc(2u * mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenNonContiguousMappingsWhenGettingRangeHandleThenAddressNotFoundIsReturned) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0x400000);
    const size_t mappingSize = 4096u;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(2u * mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_ADDRESS_NOT_FOUND, result);

    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenReservationMappingWithPhysicalOffsetWhenGettingRangeHandleThenChunkHandleCarriesOffsetAndReservedDeviceType) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0xC00000);
    const size_t mappingSize = 4096u;
    const uint64_t physicalOffset = 4096u;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;
    range0.mappedPhysicalOffset = physicalOffset;
    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto handlesBase = reinterpret_cast<ze_ipc_mem_handle_t *>(gRangeTransportStorage + sizeof(IpcRangeTransportHeader));
    IpcOpaqueMemoryData chunk{};
    std::memcpy(&chunk, handlesBase[0].data, sizeof(chunk));
    EXPECT_EQ(physicalOffset, static_cast<uint64_t>(chunk.poolOffset));
    EXPECT_EQ(static_cast<uint8_t>(InternalIpcMemoryType::reservedDeviceMemory), chunk.memoryType);

    IpcOpaqueMemoryData rangeData{};
    std::memcpy(&rangeData, ipcHandle.data, sizeof(rangeData));
    EXPECT_EQ(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport), rangeData.memoryType);

    auto header = reinterpret_cast<const IpcRangeTransportHeader *>(gRangeTransportStorage);
    EXPECT_EQ(0u, header->rangeIsHost);

    EXPECT_EQ(ZE_RESULT_SUCCESS, exportContext->putIpcMemHandle(ipcHandle));
    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenHostReservationMappingWhenGettingRangeHandleThenTransportHeaderIsTaggedHostAndChunksAreHostType) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0xC80000);
    const size_t mappingSize = 4096u;
    const uint64_t physicalOffset = 4096u;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    alloc0.setAllocationType(NEO::AllocationType::bufferHostMemory);
    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;
    range0.mappedPhysicalOffset = physicalOffset;
    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto handlesBase = reinterpret_cast<ze_ipc_mem_handle_t *>(gRangeTransportStorage + sizeof(IpcRangeTransportHeader));
    IpcOpaqueMemoryData chunk{};
    std::memcpy(&chunk, handlesBase[0].data, sizeof(chunk));
    EXPECT_EQ(physicalOffset, static_cast<uint64_t>(chunk.poolOffset));
    EXPECT_EQ(static_cast<uint8_t>(InternalIpcMemoryType::reservedHostMemory), chunk.memoryType);

    auto header = reinterpret_cast<const IpcRangeTransportHeader *>(gRangeTransportStorage);
    EXPECT_EQ(1u, header->rangeIsHost);

    EXPECT_EQ(ZE_RESULT_SUCCESS, exportContext->putIpcMemHandle(ipcHandle));
    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenRangeSizeOverflowingAddressSpaceWhenGettingRangeHandleThenNoChunksAreCollectedAndAddressNotFoundIsReturned) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0xFFFFFFFFFFFF0000ull);
    const size_t mappingSize = 4096u;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(0x20000u);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->callGetIpcRangeHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_ADDRESS_NOT_FOUND, result);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenContiguousMappingWithNullAllocationWhenGettingRangeHandleThenAddressNotFoundIsReturned) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0xB00000);
    const size_t mappingSize = 4096u;

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = nullptr;

    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->callGetIpcRangeHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_ADDRESS_NOT_FOUND, result);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenFabricAccessibleRangeDescWhenGettingRangeHandleThenChunkHandlesCarryReservedHandleData) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0xD00000);
    const size_t mappingSize = 4096u;

    ReservedDataRecordingAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});

    ze_ipc_mem_handle_type_ext_desc_t typeDesc = {};
    typeDesc.stype = ZE_STRUCTURE_TYPE_IPC_MEM_HANDLE_TYPE_EXT_DESC;
    typeDesc.pNext = nullptr;
    typeDesc.typeFlags = ZE_IPC_MEM_HANDLE_TYPE_FLAG_FABRIC_ACCESSIBLE;

    auto desc = buildRangeDesc(mappingSize);
    desc.pNext = &typeDesc;
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(alloc0.reservedHandleDataWasSet);
    EXPECT_TRUE(exportContext->callIsIpcRangeHandle(ipcHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, exportContext->putIpcMemHandle(ipcHandle));
    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenAllocationWhenEncodingRangeHandleWithOpaqueSettingsThenSuccessIsReturned) {
    auto exportContext = createExportContext();
    NEO::MockGraphicsAllocation alloc(neoDevice->getRootDeviceIndex(), reinterpret_cast<void *>(0x700000), 4096u);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->callEncodeIpcHandleForRangeAllocation(&alloc, 0x700000u, ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(IpcMemHandleRangeTest, givenOpaqueHandlingDisabledWhenEncodingRangeAllocationThenOpaqueHandleDataIsStillProduced) {
    auto exportContext = createExportContext();
    exportContext->setUseOpaqueHandle(0u);
    NEO::MockGraphicsAllocation alloc(neoDevice->getRootDeviceIndex(), reinterpret_cast<void *>(0x800000), 4096u);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->callEncodeIpcHandleForRangeAllocation(&alloc, 0x800000u, ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &opaqueData = *reinterpret_cast<IpcOpaqueMemoryData *>(ipcHandle.data);
    EXPECT_EQ(NEO::SysCalls::getCurrentProcessId(), opaqueData.processId);
}

TEST_F(IpcMemHandleRangeTest, givenAllocationFailingInternalHandleWhenEncodingRangeHandleThenOutOfHostMemoryIsReturned) {
    auto exportContext = createExportContext();
    MockRangeFailingGraphicsAllocation alloc(neoDevice->getRootDeviceIndex(), reinterpret_cast<void *>(0x900000), 4096u);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->callEncodeIpcHandleForRangeAllocation(&alloc, 0x900000u, ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
}

TEST_F(IpcMemHandleRangeTest, givenReservedHandleDataWhenEncodingRangeAllocationThenReservedDataIsForwarded) {
    auto exportContext = createExportContext();
    ReservedDataRecordingAllocation alloc(neoDevice->getRootDeviceIndex(), reinterpret_cast<void *>(0x950000), 4096u);
    ze_ipc_mem_handle_t ipcHandle = {};
    uint8_t reservedHandleData[sizeof(IpcOpaqueMemoryData::reservedHandleData)] = {0};
    auto result = exportContext->callEncodeIpcHandleForRangeAllocation(&alloc, 0x950000u, ipcHandle,
                                                                       static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory), 0u, reservedHandleData);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(alloc.reservedHandleDataWasSet);
}

TEST_F(IpcMemHandleRangeTest, givenRangeHandleWhenPuttingIpcMemHandleThenSuccessIsReturned) {
    auto ipcHandle = buildRangeHandle();
    auto result = context->putIpcMemHandle(ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(IpcMemHandleRangeTest, givenMatchingAndNonMatchingEntriesWhenPuttingRangeHandleThenOnlyMatchingEntryIsErased) {
    auto exportContext = createExportContext();

    exportContext->injectRangeTransport(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(0x4000), 111u, {});
    exportContext->injectRangeTransport(reinterpret_cast<void *>(0x2000), reinterpret_cast<void *>(0x5000), static_cast<uint64_t>(transportHandleFd), {});
    ASSERT_EQ(2u, exportContext->rangeTransportCount());

    auto ipcHandle = buildRangeHandle();
    auto result = exportContext->putIpcMemHandle(ipcHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, exportContext->rangeTransportCount());
}

TEST_F(IpcMemHandleRangeTest, givenOnlyNonMatchingEntryWhenPuttingRangeHandleThenEntryIsRetained) {
    auto exportContext = createExportContext();

    exportContext->injectRangeTransport(reinterpret_cast<void *>(0x3000), reinterpret_cast<void *>(0x6000), 111u, {});
    ASSERT_EQ(1u, exportContext->rangeTransportCount());

    auto ipcHandle = buildRangeHandle();
    auto result = exportContext->putIpcMemHandle(ipcHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, exportContext->rangeTransportCount());
}

TEST_F(IpcMemHandleRangeTest, givenExportedRangeTransportWhenFreeingBaseAddressViaMemFreeThenInvalidArgumentIsReturnedAndTransportIsRetained) {
    auto exportContext = createExportContext();
    void *baseAddress = reinterpret_cast<void *>(0x1000);
    exportContext->injectRangeTransport(baseAddress, reinterpret_cast<void *>(0x4000), static_cast<uint64_t>(transportHandleFd), {});
    ASSERT_EQ(1u, exportContext->rangeTransportCount());

    auto result = exportContext->freeMem(baseAddress);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(1u, exportContext->rangeTransportCount());
}

TEST_F(IpcMemHandleRangeTest, givenExportedRangeTransportWhenFreeingDifferentAddressThenTransportIsRetained) {
    auto exportContext = createExportContext();
    void *baseAddress = reinterpret_cast<void *>(0x2000);
    exportContext->injectRangeTransport(baseAddress, reinterpret_cast<void *>(0x4000), static_cast<uint64_t>(transportHandleFd), {});
    ASSERT_EQ(1u, exportContext->rangeTransportCount());

    auto result = exportContext->freeMem(reinterpret_cast<void *>(0x9999));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(1u, exportContext->rangeTransportCount());
}

TEST_F(IpcMemHandleRangeTest, givenExportedRangeTransportWhenFreeingReservedVirtualMemoryThenTransportIsReleasedAndSuccessIsReturned) {
    auto exportContext = createExportContext();

    const size_t size = 4096u;
    void *baseAddress = reinterpret_cast<void *>(0x1000);
    auto *heapReservation = new NEO::VirtualMemoryReservation();
    heapReservation->rootDeviceIndex = neoDevice->getRootDeviceIndex();
    heapReservation->reservationSize = size;
    heapReservation->reservationBase = 0u;
    heapReservation->reservationTotalSize = 0u;
    heapReservation->isSvmReservation = true;
    {
        auto memoryManager = driverHandle->getMemoryManager();
        auto lock = memoryManager->lockVirtualMemoryReservationMap();
        memoryManager->getVirtualMemoryReservationMap()[baseAddress] = heapReservation;
    }

    exportContext->injectRangeTransport(baseAddress, reinterpret_cast<void *>(0x4000), static_cast<uint64_t>(transportHandleFd), {});
    ASSERT_EQ(1u, exportContext->rangeTransportCount());

    auto result = exportContext->freeVirtualMem(baseAddress, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());
}

TEST_F(IpcMemHandleRangeTest, givenNonMatchingAndMatchingRangeEntriesWhenReleasingRangeTransportForPtrThenNonMatchingEntryIsSkippedAndMatchingIsReleased) {
    auto exportContext = createExportContext();

    void *firstBase = reinterpret_cast<void *>(0x1000);
    void *secondBase = reinterpret_cast<void *>(0x2000);
    exportContext->injectRangeTransport(firstBase, reinterpret_cast<void *>(0x4000), 111u, {});
    exportContext->injectRangeTransport(secondBase, reinterpret_cast<void *>(0x5000), 222u, {});
    ASSERT_EQ(2u, exportContext->rangeTransportCount());

    auto released = exportContext->callReleaseIpcRangeTransportForPtr(secondBase);
    EXPECT_TRUE(released);
    EXPECT_EQ(1u, exportContext->rangeTransportCount());
}

TEST_F(IpcMemHandleRangeTest, givenNonRangeOpaqueHandleWhenCheckingIsIpcRangeHandleThenFalseIsReturned) {
    auto exportContext = createExportContext();
    auto hostHandle = buildOpaqueHandle(300, static_cast<uint8_t>(InternalIpcMemoryType::hostUnifiedMemory));
    auto deviceHandle = buildOpaqueHandle(301, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory));
    auto rangeHandle = buildRangeHandle();

    EXPECT_FALSE(exportContext->callIsIpcRangeHandle(hostHandle));
    EXPECT_FALSE(exportContext->callIsIpcRangeHandle(deviceHandle));
    EXPECT_TRUE(exportContext->callIsIpcRangeHandle(rangeHandle));
}

TEST_F(IpcMemHandleRangeTest, givenRangeHandleWhenOpeningIpcMemHandleThenHandlesAreImportedInStoredOrder) {
    auto recordingMemoryManager = new RecordingRangeMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(recordingMemoryManager);
    delete currMemoryManager;
    currMemoryManager = recordingMemoryManager;

    auto importContext = createImportContext();

    const uint32_t numHandles = 3u;
    buildRangeTransport(numHandles);
    enableChunkImportMocks();

    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> closeBackup{&NEO::SysCalls::sysCallsClose};
    NEO::SysCalls::sysCallsClose = [](int fd) -> int {
        gClosedImportFds.insert(fd);
        return 0;
    };

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    EXPECT_TRUE(importContext->getMemHandlePtrCalled);
    ASSERT_EQ(numHandles, recordingMemoryManager->recordedHandles.size());
    for (uint32_t i = 0; i < numHandles; i++) {
        EXPECT_EQ(static_cast<NEO::osHandle>(baseHandleValue + i), recordingMemoryManager->recordedHandles[i]);
        EXPECT_EQ(1u, gClosedImportFds.count(static_cast<int>(baseHandleValue + i)));
    }
    EXPECT_TRUE(driverHandle->opaqueHandleImportCache.empty());
    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto rangeAllocation = allocData->gpuAllocations.getDefaultGraphicsAllocation();
    ASSERT_NE(nullptr, rangeAllocation);
    EXPECT_EQ(NEO::Sharing::nonSharedResource, rangeAllocation->peekSharedHandle());
    EXPECT_TRUE(rangeAllocation->getIsImported());

    importContext->closeIpcMemHandle(ptr);
}

TEST_F(IpcMemHandleRangeTest, givenTransportImportFailureWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    importContext->transportImportFails = true;
    buildRangeTransport(2u);

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_TRUE(importContext->getMemHandlePtrCalled);
}

TEST_F(IpcMemHandleRangeTest, givenMismatchedTransportVersionWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    buildRangeTransport(2u);
    reinterpret_cast<IpcRangeTransportHeader *>(gRangeTransportStorage)->version = ipcRangeTransportVersion + 1u;

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(IpcMemHandleRangeTest, givenTransportHeaderWithBadMagicWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    buildRangeTransport(1u);
    reinterpret_cast<IpcRangeTransportHeader *>(gRangeTransportStorage)->magic = ipcRangeHandleMagic + 1u;

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(IpcMemHandleRangeTest, givenTransportHeaderWithZeroHandlesWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    buildRangeTransport(0u);

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(IpcMemHandleRangeTest, givenTransportImportWithoutGraphicsAllocationWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    importContext->transportImportWithoutAllocation = true;
    buildRangeTransport(2u);

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(IpcMemHandleRangeTest, givenTransportHeaderClaimingMoreHandlesThanImportedTransportSizeWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    buildRangeTransport(2u);
    reinterpret_cast<IpcRangeTransportHeader *>(gRangeTransportStorage)->numHandles = std::numeric_limits<uint32_t>::max();

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(IpcMemHandleRangeTest, givenTransportHeaderExactlyFillingImportedTransportSizeWhenOpeningRangeHandleThenHandlesAreRead) {
    auto importContext = createImportContext();
    buildRangeTransport(2u);
    importContext->transportAllocationSize = sizeof(IpcRangeTransportHeader) + 2u * sizeof(ze_ipc_mem_handle_t);

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, ptr);
    importContext->closeIpcMemHandle(ptr);
}

TEST_F(IpcMemHandleRangeTest, givenImportFdHandlesFailureWhenOpeningRangeHandleThenOutOfDeviceMemoryIsReturnedAndImportedHandlesAreReleased) {
    auto importContext = createImportContext();
    buildRangeTransport(2u);
    enableChunkImportMocks();

    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> closeBackup{&NEO::SysCalls::sysCallsClose};
    NEO::SysCalls::sysCallsClose = [](int fd) -> int { gClosedImportFds.insert(fd); return 0; };

    static_cast<MemoryManagerOpenIpcMock *>(currMemoryManager)->failOnCreateGraphicsAllocationFromSharedHandle = true;

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, result);

    EXPECT_EQ(1u, gClosedImportFds.count(static_cast<int>(baseHandleValue)));
    EXPECT_EQ(1u, gClosedImportFds.count(static_cast<int>(baseHandleValue + 1u)));
    EXPECT_TRUE(driverHandle->opaqueHandleImportCache.empty());
}

TEST_F(IpcMemHandleRangeTest, givenChunkImportFailingMidRangeWhenOpeningRangeHandleThenPreviouslyImportedHandlesAreReleased) {
    auto importContext = createImportContext();
    const uint32_t numHandles = 2u;
    buildRangeTransport(numHandles);
    NEO::SysCalls::sysCallsPidfdOpen = [](pid_t, unsigned int) -> int { return 77; };
    NEO::SysCalls::sysCallsPidfdGetfd = [](int, int fd, unsigned int) -> int {
        return fd == static_cast<int>(baseHandleValue + 1u) ? -1 : fd;
    };

    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> closeBackup{&NEO::SysCalls::sysCallsClose};
    NEO::SysCalls::sysCallsClose = [](int fd) -> int { gClosedImportFds.insert(fd); return 0; };

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(1u, gClosedImportFds.count(static_cast<int>(baseHandleValue)));
    EXPECT_TRUE(driverHandle->opaqueHandleImportCache.empty());
}

TEST_F(IpcMemHandleRangeTest, givenOpaqueHandlingDisabledWhenOpeningRangeHandleThenUnsupportedFeatureIsReturned) {
    auto importContext = createImportContext();
    importContext->setUseOpaqueHandle(0u);

    buildRangeTransport(1u);

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(IpcMemHandleRangeTest, givenFailingOpaqueImportWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    buildRangeTransport(1u);
    NEO::SysCalls::sysCallsPidfdOpen = [](pid_t, unsigned int) -> int { return 77; };
    NEO::SysCalls::sysCallsPidfdGetfd = [](int, int, unsigned int) -> int { return -1; };

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(IpcMemHandleRangeTest, givenNonOpaqueStoredHandlesWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    const uint32_t numHandles = 2u;
    buildRangeTransport(numHandles, false);

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(IpcMemHandleRangeTest, givenNullPtrOutputWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    auto ipcHandle = buildRangeHandle();
    auto result = importContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(IpcMemHandleRangeTest, givenImportReturningNullAllocationWhenOpeningRangeHandleThenSharedHandleIsNotSetAndSuccessIsReturned) {
    auto nullAllocMemoryManager = new NullRangeAllocMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(nullAllocMemoryManager);
    delete currMemoryManager;
    currMemoryManager = nullAllocMemoryManager;

    auto importContext = createImportContext();

    const uint32_t numHandles = 2u;
    buildRangeTransport(numHandles);
    enableChunkImportMocks();

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    EXPECT_TRUE(nullAllocMemoryManager->importFdHandlesCalled);
}

TEST_F(IpcMemHandleRangeTest, givenStoredChunkHandleWithNonDeviceMemoryTypeWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    const uint32_t numHandles = 1u;
    buildRangeTransport(numHandles, true, static_cast<uint8_t>(InternalIpcMemoryType::hostUnifiedMemory));

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(IpcMemHandleRangeTest, givenReservedDeviceChunksWithPhysicalOffsetWhenOpeningRangeHandleThenOffsetsArePassedToImport) {
    auto nullAllocMemoryManager = new NullRangeAllocMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(nullAllocMemoryManager);
    delete currMemoryManager;
    currMemoryManager = nullAllocMemoryManager;

    auto importContext = createImportContext();

    const uint32_t numHandles = 2u;
    const uint64_t physicalOffset = 4096u;
    buildRangeTransport(numHandles, true, static_cast<uint8_t>(InternalIpcMemoryType::reservedDeviceMemory), physicalOffset);
    enableChunkImportMocks();

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(nullAllocMemoryManager->importFdHandlesCalled);
    ASSERT_EQ(numHandles, nullAllocMemoryManager->lastPhysicalOffsets.size());
    EXPECT_EQ(physicalOffset, nullAllocMemoryManager->lastPhysicalOffsets[0]);
    EXPECT_EQ(physicalOffset, nullAllocMemoryManager->lastPhysicalOffsets[1]);
}

TEST_F(IpcMemHandleRangeTest, givenHostRangeHandleWhenOpeningThenHostMergePathIsSelectedAndDeviceImportIsNotUsed) {
    auto nullAllocMemoryManager = new NullRangeAllocMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(nullAllocMemoryManager);
    delete currMemoryManager;
    currMemoryManager = nullAllocMemoryManager;

    auto importContext = createImportContext();

    const uint32_t numHandles = 2u;
    buildRangeTransport(numHandles, true, static_cast<uint8_t>(InternalIpcMemoryType::reservedHostMemory), 0u, 0u, true);
    enableChunkImportMocks();

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_FALSE(nullAllocMemoryManager->importFdHandlesCalled);
}

TEST_F(IpcMemHandleRangeTest, givenHostRangeHandleWhenHostMergeSucceedsThenSvmAllocIsInsertedAndSuccessIsReturned) {
    auto hostAllocMemoryManager = new HostRangeAllocMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(hostAllocMemoryManager);
    delete currMemoryManager;
    currMemoryManager = hostAllocMemoryManager;

    auto importContext = createImportContext();

    const uint32_t numHandles = 2u;
    const uint64_t physicalOffset = 4096u;
    buildRangeTransport(numHandles, true, static_cast<uint8_t>(InternalIpcMemoryType::reservedHostMemory), physicalOffset, 0u, true);
    enableChunkImportMocks();

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);
    EXPECT_TRUE(hostAllocMemoryManager->createHostAllocationCalled);
    ASSERT_EQ(numHandles, hostAllocMemoryManager->lastPhysicalOffsets.size());
    EXPECT_EQ(physicalOffset, hostAllocMemoryManager->lastPhysicalOffsets[0]);

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, allocData->memoryType);

    importContext->closeIpcMemHandle(ptr);
}

TEST_F(IpcMemHandleRangeTest, givenRangeHandleWithLeadingOffsetWhenOpeningThenReturnedPtrIsShiftedByLeadingOffset) {
    auto hostAllocMemoryManager = new HostRangeAllocMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(hostAllocMemoryManager);
    delete currMemoryManager;
    currMemoryManager = hostAllocMemoryManager;

    auto importContext = createImportContext();

    const uint32_t numHandles = 2u;
    const uint64_t leadingOffset = 0x40u;
    buildRangeTransport(numHandles, true, static_cast<uint8_t>(InternalIpcMemoryType::reservedHostMemory), 0u, leadingOffset, true);
    enableChunkImportMocks();

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(reinterpret_cast<void *>(0x9000u + leadingOffset), ptr);

    importContext->closeIpcMemHandle(ptr);
}

TEST_F(IpcMemHandleRangeTest, givenUncachedBiasFlagWhenOpeningDeviceRangeHandleThenUncachedBiasIsForwardedToImport) {
    auto nullAllocMemoryManager = new NullRangeAllocMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(nullAllocMemoryManager);
    delete currMemoryManager;
    currMemoryManager = nullAllocMemoryManager;

    auto importContext = createImportContext();

    const uint32_t numHandles = 2u;
    buildRangeTransport(numHandles);
    enableChunkImportMocks();

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(nullAllocMemoryManager->importFdHandlesCalled);
    EXPECT_TRUE(nullAllocMemoryManager->lastUncachedBias);
}

TEST_F(IpcMemHandleRangeTest, givenHostRangeHandleWithDeviceChunkTypeWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    auto importContext = createImportContext();
    const uint32_t numHandles = 1u;
    buildRangeTransport(numHandles, true, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory), 0u, 0u, true);

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(IpcMemHandleRangeTest, givenImplicitScalingCapableDeviceWhenOpeningDeviceRangeHandleThenRootDeviceIsUsedAndSuccessIsReturned) {
    auto nullAllocMemoryManager = new NullRangeAllocMemoryManager(*neoDevice->executionEnvironment);
    driverHandle->setMemoryManager(nullAllocMemoryManager);
    delete currMemoryManager;
    currMemoryManager = nullAllocMemoryManager;

    auto importContext = createImportContext();

    MockDeviceImp implicitScalingDevice(neoDevice);
    implicitScalingDevice.implicitScalingCapable = true;

    const uint32_t numHandles = 2u;
    buildRangeTransport(numHandles);
    enableChunkImportMocks();

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->openIpcMemHandle(implicitScalingDevice.toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(nullAllocMemoryManager->importFdHandlesCalled);
}

TEST_F(IpcMemHandleRangeTest, givenPtrInGapBetweenTwoReservationsWhenGettingRangeHandleThenPreviousReservationIsInspectedAndInvalidArgumentIsReturned) {
    auto exportContext = createExportContext();
    auto memoryManager = driverHandle->getMemoryManager();

    void *firstBase = reinterpret_cast<void *>(0x400000u);
    void *secondBase = reinterpret_cast<void *>(0x900000u);
    NEO::VirtualMemoryReservation firstReservation{};
    firstReservation.rootDeviceIndex = neoDevice->getRootDeviceIndex();
    firstReservation.virtualAddressRange.address = reinterpret_cast<uint64_t>(firstBase);
    firstReservation.virtualAddressRange.size = 0x1000u;
    NEO::VirtualMemoryReservation secondReservation{};
    secondReservation.rootDeviceIndex = neoDevice->getRootDeviceIndex();
    secondReservation.virtualAddressRange.address = reinterpret_cast<uint64_t>(secondBase);
    secondReservation.virtualAddressRange.size = 0x1000u;
    {
        auto lock = memoryManager->lockVirtualMemoryReservationMap();
        memoryManager->getVirtualMemoryReservationMap()[firstBase] = &firstReservation;
        memoryManager->getVirtualMemoryReservationMap()[secondBase] = &secondReservation;
    }

    void *gapPtr = reinterpret_cast<void *>(0x500000u);
    auto desc = buildRangeDesc(0x1000u);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->callGetIpcRangeHandle(gapPtr, &desc, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    {
        auto lock = memoryManager->lockVirtualMemoryReservationMap();
        memoryManager->getVirtualMemoryReservationMap().erase(firstBase);
        memoryManager->getVirtualMemoryReservationMap().erase(secondBase);
    }
}

TEST_F(IpcMemHandleRangeTest, givenNtHandleTypeRangeTransportWhenCheckingIsIpcRangeHandleThenTrueIsReturned) {
    auto exportContext = createExportContext();
    ze_ipc_mem_handle_t handle = {};
    IpcOpaqueMemoryData opaque{};
    opaque.handle.fd = 400;
    opaque.opaqueHandle.fd = 400;
    opaque.type = IpcHandleType::ntHandle;
    opaque.memoryType = static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport);
    std::memcpy(handle.data, &opaque, sizeof(opaque));

    EXPECT_TRUE(exportContext->callIsIpcRangeHandle(handle));
}

TEST_F(IpcMemHandleRangeTest, givenUnknownHandleTypeWhenCheckingIsIpcRangeHandleThenFalseIsReturned) {
    auto exportContext = createExportContext();
    ze_ipc_mem_handle_t handle = {};
    IpcOpaqueMemoryData opaque{};
    opaque.handle.fd = 401;
    opaque.opaqueHandle.fd = 401;
    opaque.type = IpcHandleType::maxHandle;
    opaque.memoryType = static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport);
    std::memcpy(handle.data, &opaque, sizeof(opaque));

    EXPECT_FALSE(exportContext->callIsIpcRangeHandle(handle));
}

TEST_F(IpcMemHandleRangeTest, givenNonMatchingAndNonFabricExtDescsWhenGettingRangeHandleThenChainIsWalkedWithoutFabricData) {
    installExportSvmManager();
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0xE00000);
    const size_t mappingSize = 4096u;

    ReservedDataRecordingAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;
    installReservation(basePtr, {&range0});

    ze_ipc_mem_handle_type_ext_desc_t typeDesc = {};
    typeDesc.stype = ZE_STRUCTURE_TYPE_IPC_MEM_HANDLE_TYPE_EXT_DESC;
    typeDesc.pNext = nullptr;
    typeDesc.typeFlags = 0u;

    ze_base_desc_t unrelatedDesc = {};
    unrelatedDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;
    unrelatedDesc.pNext = &typeDesc;

    auto desc = buildRangeDesc(mappingSize);
    desc.pNext = &unrelatedDesc;
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(alloc0.reservedHandleDataWasSet);
    EXPECT_TRUE(exportContext->callIsIpcRangeHandle(ipcHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, exportContext->putIpcMemHandle(ipcHandle));
    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenZeroMaxHandleCountWhenOpeningRangeHandleThenUnsupportedFeatureIsReturned) {
    auto importContext = createImportContext();
    importContext->maxIpcRangeHandleCount = 0u;

    auto ipcHandle = buildRangeHandle();
    void *ptr = nullptr;
    auto result = importContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, 0u, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(IpcMemHandleRangeTest, givenTransportAllocationWithoutGraphicsAllocationWhenGettingRangeHandleThenOutOfHostMemoryIsReturned) {
    installExportSvmManager();
    rangeSvmManager->returnUntrackedTransport = true;
    auto exportContext = createExportContext();

    void *basePtr = reinterpret_cast<void *>(0x1200000);
    const size_t mappingSize = 4096u;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;
    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = exportContext->getIpcMemHandle(basePtr, &desc, &ipcHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());

    clearReservation();
}

TEST_F(IpcMemHandleRangeTest, givenRangeTransportWithNullTransportPtrWhenReleasingByPtrThenReleaseIsHandledAndEntryIsErased) {
    auto exportContext = createExportContext();

    void *baseAddress = reinterpret_cast<void *>(0x1300000);
    exportContext->injectRangeTransport(baseAddress, nullptr, 333u, {});
    ASSERT_EQ(1u, exportContext->rangeTransportCount());

    auto released = exportContext->callReleaseIpcRangeTransportForPtr(baseAddress);
    EXPECT_TRUE(released);
    EXPECT_EQ(0u, exportContext->rangeTransportCount());
}

} // namespace ult
} // namespace L0

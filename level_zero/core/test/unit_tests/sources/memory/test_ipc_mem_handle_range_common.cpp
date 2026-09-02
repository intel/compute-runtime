/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"

#include <cstring>
#include <vector>

namespace L0 {
namespace ult {

namespace {
constexpr uint32_t commonChunkHandleBase = 300u;
uint8_t gCommonTransportStorage[4096] = {};
} // namespace

class FailingInternalHandleAllocation : public NEO::MockGraphicsAllocation {
  public:
    FailingInternalHandleAllocation(uint32_t rootDeviceIndex, void *buffer, size_t sizeIn)
        : NEO::MockGraphicsAllocation(rootDeviceIndex, buffer, sizeIn) {}

    int createInternalHandle(NEO::MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle, void *reservedHandleData) override {
        return -1;
    }
};

class ReservedDataRecordingRangeAllocation : public NEO::MockGraphicsAllocation {
  public:
    ReservedDataRecordingRangeAllocation(uint32_t rootDeviceIndex, void *buffer, size_t sizeIn)
        : NEO::MockGraphicsAllocation(rootDeviceIndex, buffer, sizeIn) {}

    int createInternalHandle(NEO::MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle, void *reservedHandleData) override {
        reservedHandleDataWasSet = reservedHandleData != nullptr;
        handle = 0u;
        return 0;
    }

    bool reservedHandleDataWasSet = false;
};

class CommonRangeSvmManager : public NEO::MockSVMAllocsManager {
  public:
    CommonRangeSvmManager(NEO::MemoryManager *memoryManager) : NEO::MockSVMAllocsManager(memoryManager) {}

    void *createHostUnifiedMemoryAllocation(size_t size, const NEO::UnifiedMemoryProperties &memoryProperties) override {
        NEO::MockGraphicsAllocation *alloc = failTransportInternalHandle
                                                 ? new FailingInternalHandleAllocation(0u, gCommonTransportStorage, size)
                                                 : new NEO::MockGraphicsAllocation(0u, gCommonTransportStorage, size);
        NEO::SvmAllocationData allocData(0u);
        allocData.gpuAllocations.addAllocation(alloc);
        allocData.cpuAllocation = nullptr;
        allocData.size = size;
        allocData.memoryType = InternalMemoryType::hostUnifiedMemory;
        allocData.device = nullptr;
        allocData.setAllocId(++this->allocationsCounter);
        this->insertSVMAlloc(gCommonTransportStorage, allocData);
        return gCommonTransportStorage;
    }

    bool failTransportInternalHandle = false;
};

class CommonRangeMemoryManager : public MemoryManagerOpenIpcMock {
  public:
    CommonRangeMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerOpenIpcMock(executionEnvironment) {}

    void *importFdHandles(NEO::Device *neoDevice, NEO::SVMAllocsManager *svmAllocsManager, const std::vector<NEO::osHandle> &handles, void *basePointer, NEO::GraphicsAllocation **pAlloc, NEO::SvmAllocationData &mappedPeerAllocData, bool compressedMemory, bool uncachedBias, const std::vector<uint64_t> &physicalOffsets) override {
        lastUncachedBias = uncachedBias;
        if (pAlloc != nullptr) {
            *pAlloc = nullptr;
        }
        if (failImportFdHandles) {
            return nullptr;
        }
        return &mergedBaseStorage;
    }

    NEO::GraphicsAllocation *createHostAllocationFromMultipleSharedHandles(const std::vector<NEO::osHandle> &handles, NEO::AllocationProperties &properties, const std::vector<uint64_t> &physicalOffsets, bool reuseSharedAllocation) override {
        createHostAllocationCalled = true;
        lastHostPhysicalOffsets = physicalOffsets;
        if (failCreateHostAllocation) {
            return nullptr;
        }
        return new NEO::MockGraphicsAllocation(properties.rootDeviceIndex, reinterpret_cast<void *>(0x9000), MemoryConstants::pageSize);
    }

    bool lastUncachedBias = false;
    bool failImportFdHandles = false;
    bool createHostAllocationCalled = false;
    bool failCreateHostAllocation = false;
    std::vector<uint64_t> lastHostPhysicalOffsets;
    uint64_t mergedBaseStorage = 0u;
};

struct CommonRangeContext : public L0::Context {
    CommonRangeContext(L0::DriverHandle *inDriverHandle) : L0::Context(inDriverHandle) {
        driverHandle = inDriverHandle;
        settings.enableIpcHandleSharingByDefault = true;
        settings.handleType = IpcHandleType::fdHandle;
    }

    void setUseOpaqueHandle(uint8_t value) { settings.useOpaqueHandle = value; }

    using L0::Context::IpcRangeTransportEntry;
    using L0::Context::ipcRangeTransports;
    using L0::Context::ipcRangeTransportsPresent;

    ze_result_t callEncode(NEO::GraphicsAllocation *alloc, ze_ipc_mem_handle_t &ipcHandle) {
        return encodeIpcHandleForRangeAllocation(alloc, 0x1000u, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory), 0u, nullptr, ipcHandle);
    }
    ze_result_t callOpenIpcRangeHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t &ipcHandle, ze_ipc_memory_flags_t flags, void **pptr) {
        return openIpcRangeHandle(hDevice, ipcHandle, flags, pptr);
    }
    void callReleaseIpcRangeTransport(const void *transportPtr) { releaseIpcRangeTransport(transportPtr); }
    bool callReleaseIpcRangeTransportForPtr(const void *ptr) { return releaseIpcRangeTransportForPtr(ptr); }
    bool callIsIpcRangeHandle(const ze_ipc_mem_handle_t &ipcHandle) const { return isIpcRangeHandle(ipcHandle); }
    uint32_t callRealMaxIpcRangeHandleCount() { return Context::getMaxIpcRangeHandleCount(); }

    uint32_t getMaxIpcRangeHandleCount() override { return maxIpcRangeHandleCount; }

    ze_result_t freeMem(const void *ptr) override {
        freeMemCalls++;
        if (driverHandle->svmAllocsManager->getSVMAlloc(ptr) == nullptr) {
            return ZE_RESULT_SUCCESS;
        }
        return Context::freeMem(ptr);
    }

    std::pair<NEO::GraphicsAllocation *, void *> getMemHandlePtr(ze_device_handle_t hDevice, uint64_t handle, NEO::AllocationType allocationType, bool isHostIpcAllocation, unsigned int processId, ze_ipc_memory_flags_t flags, uint64_t cacheID, void *reservedHandleData, bool compressedMemory, bool isOpaqueHandle, uint64_t physicalOffset) override {
        transportAllocation = std::make_unique<NEO::MockGraphicsAllocation>(0u, gCommonTransportStorage, sizeof(gCommonTransportStorage));
        return {transportAllocation.get(), gCommonTransportStorage};
    }

    void getDataFromIpcHandle(ze_device_handle_t hDevice, const ze_ipc_mem_handle_t &ipcHandle, uint64_t &handle, uint8_t &type, unsigned int &processId, uint64_t &poolOffset, uint64_t &cacheID, void *&reservedHandleData, bool &compressedMemory, bool &isOpaqueHandle) override {
        IpcOpaqueMemoryData opaqueData{};
        std::memcpy(&opaqueData, ipcHandle.data, sizeof(opaqueData));
        handle = opaqueData.handle.reserved;
        type = opaqueData.memoryType;
        processId = opaqueData.processId;
        poolOffset = opaqueData.poolOffset;
        cacheID = 0u;
        reservedHandleData = nullptr;
        compressedMemory = false;
        isOpaqueHandle = true;
    }

    OpaqueHandleImportResult importOpaqueHandleWithFallback(uint64_t handle, unsigned int processId, uint64_t cacheID, void *reservedHandleData, NEO::Device *neoDevice) override {
        return {handle, true};
    }

    void releaseImportedRangeChunkHandles(const std::vector<std::pair<uint64_t, uint64_t>> &importedChunks) override {
        releaseImportedCalls++;
        if (importedChunks.empty()) {
            Context::releaseImportedRangeChunkHandles(importedChunks);
        }
    }

    std::unique_ptr<NEO::MockGraphicsAllocation> transportAllocation;
    uint32_t maxIpcRangeHandleCount = 1024u;
    uint32_t freeMemCalls = 0u;
    uint32_t releaseImportedCalls = 0u;
};

struct IpcMemHandleRangeCommonTest : public MemoryOpenIpcHandleTest {
    void SetUp() override {
        MemoryOpenIpcHandleTest::SetUp();
        std::memset(gCommonTransportStorage, 0, sizeof(gCommonTransportStorage));
        rangeMemoryManager = new CommonRangeMemoryManager(*neoDevice->executionEnvironment);
        driverHandle->setMemoryManager(rangeMemoryManager);
        delete currMemoryManager;
        currMemoryManager = rangeMemoryManager;

        previousSvmManager = driverHandle->svmAllocsManager;
        rangeSvmManager = std::make_unique<CommonRangeSvmManager>(driverHandle->getMemoryManager());
        driverHandle->svmAllocsManager = rangeSvmManager.get();

        rangeContext = std::make_unique<CommonRangeContext>(driverHandle.get());
        rangeContext->getDevices().insert(std::make_pair(device->getRootDeviceIndex(), device->toHandle()));
        rangeContext->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
        rangeContext->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});
    }

    void TearDown() override {
        if (installedPoolFacade != nullptr) {
            installedPoolFacade->pool.reset(nullptr);
            installedPoolFacade = nullptr;
        }
        clearReservation();
        rangeContext.reset();
        drainIpcHandleMap();
        driverHandle->svmAllocsManager = previousSvmManager;
        rangeSvmManager.reset();
        MemoryOpenIpcHandleTest::TearDown();
    }

    void drainIpcHandleMap() {
        auto lock = driverHandle->lockIPCHandleMap();
        auto &ipcMap = driverHandle->getIPCHandleMap();
        for (auto &entry : ipcMap) {
            delete entry.second;
        }
        ipcMap.clear();
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

    ze_ipc_mem_handle_t buildHandle(uint8_t memoryType, IpcHandleType handleType = IpcHandleType::fdHandle, uint64_t rawHandle = 0u) {
        ze_ipc_mem_handle_t handle = {};
        IpcOpaqueMemoryData opaque{};
        opaque.handle.reserved = rawHandle;
        opaque.opaqueHandle.reserved = rawHandle;
        opaque.type = handleType;
        opaque.memoryType = memoryType;
        std::memcpy(handle.data, &opaque, sizeof(opaque));
        return handle;
    }

    void buildTransport(uint32_t numHandles, uint8_t chunkMemoryType, uint64_t leadingOffset, bool rangeIsHost) {
        IpcRangeTransportHeader header{};
        header.magic = ipcRangeHandleMagic;
        header.version = ipcRangeTransportVersion;
        header.numHandles = numHandles;
        header.leadingOffset = leadingOffset;
        header.rangeIsHost = rangeIsHost ? 1u : 0u;
        std::memcpy(gCommonTransportStorage, &header, sizeof(header));

        auto handlesBase = reinterpret_cast<ze_ipc_mem_handle_t *>(gCommonTransportStorage + sizeof(header));
        for (uint32_t i = 0; i < numHandles; i++) {
            handlesBase[i] = buildHandle(chunkMemoryType, IpcHandleType::fdHandle, commonChunkHandleBase + i);
        }
    }

    void installSvmAllocation(void *ptr, size_t size) {
        svmBackingAllocation = std::make_unique<NEO::MockGraphicsAllocation>(neoDevice->getRootDeviceIndex(), ptr, size);
        NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
        allocData.gpuAllocations.addAllocation(svmBackingAllocation.get());
        allocData.cpuAllocation = nullptr;
        allocData.size = size;
        allocData.memoryType = InternalMemoryType::deviceUnifiedMemory;
        allocData.device = neoDevice;
        allocData.setAllocId(++rangeSvmManager->allocationsCounter);
        rangeSvmManager->insertSVMAlloc(ptr, allocData);
    }

    NEO::MockUsmMemAllocPool *installPooledDeviceAllocation(void *poolBase, size_t poolSize, void *ptr, size_t pooledSize) {
        auto pool = new NEO::MockUsmMemAllocPool;
        pool->callBaseCleanup = false;
        pool->pool = poolBase;
        pool->poolEnd = reinterpret_cast<uint8_t *>(poolBase) + poolSize;
        pool->allocations.insert(ptr, NEO::MockUsmMemAllocPool::AllocationInfo{reinterpret_cast<uint64_t>(ptr), pooledSize, pooledSize});
        installedPoolFacade = &static_cast<NEO::MockUsmMemAllocPoolsFacade &>(neoDevice->getDeviceUsmMemAllocPoolFacade());
        installedPoolFacade->pool.reset(pool);
        installSvmAllocation(ptr, poolSize);
        return pool;
    }

    ze_ipc_phys_mem_handle_range_ext_desc_t buildRangeDesc(size_t size) {
        ze_ipc_phys_mem_handle_range_ext_desc_t desc = {};
        desc.stype = ZE_STRUCTURE_TYPE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_DESC;
        desc.size = size;
        return desc;
    }

    CommonRangeMemoryManager *rangeMemoryManager = nullptr;
    NEO::SVMAllocsManager *previousSvmManager = nullptr;
    std::unique_ptr<CommonRangeSvmManager> rangeSvmManager;
    std::unique_ptr<CommonRangeContext> rangeContext;
    std::unique_ptr<NEO::VirtualMemoryReservation> reservation;
    std::unique_ptr<NEO::MockGraphicsAllocation> svmBackingAllocation;
    void *installedReservationBase = nullptr;
    NEO::MockUsmMemAllocPoolsFacade *installedPoolFacade = nullptr;
};

TEST_F(IpcMemHandleRangeCommonTest, givenFdTypeRangeHandleWhenCheckingIsIpcRangeHandleThenTrueIsReturned) {
    auto handle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport), IpcHandleType::fdHandle);
    EXPECT_TRUE(rangeContext->callIsIpcRangeHandle(handle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenNtTypeRangeHandleWhenCheckingIsIpcRangeHandleThenTrueIsReturned) {
    auto handle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport), IpcHandleType::ntHandle);
    EXPECT_TRUE(rangeContext->callIsIpcRangeHandle(handle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenUnknownHandleTypeWhenCheckingIsIpcRangeHandleThenFalseIsReturned) {
    auto handle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport), IpcHandleType::maxHandle);
    EXPECT_FALSE(rangeContext->callIsIpcRangeHandle(handle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenOpaqueHandlingDisabledWhenCheckingIsIpcRangeHandleThenFalseIsReturned) {
    rangeContext->setUseOpaqueHandle(0u);
    auto handle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport), IpcHandleType::fdHandle);
    EXPECT_FALSE(rangeContext->callIsIpcRangeHandle(handle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenNonRangeMemoryTypeWhenCheckingIsIpcRangeHandleThenFalseIsReturned) {
    auto handle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory), IpcHandleType::fdHandle);
    EXPECT_FALSE(rangeContext->callIsIpcRangeHandle(handle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenAllocationWhenEncodingRangeHandleThenSuccessIsReturned) {
    NEO::MockGraphicsAllocation alloc(neoDevice->getRootDeviceIndex(), reinterpret_cast<void *>(0x1000), MemoryConstants::pageSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->callEncode(&alloc, ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenAllocationFailingInternalHandleWhenEncodingRangeHandleThenOutOfHostMemoryIsReturned) {
    FailingInternalHandleAllocation alloc(neoDevice->getRootDeviceIndex(), reinterpret_cast<void *>(0x1000), MemoryConstants::pageSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, rangeContext->callEncode(&alloc, ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenZeroMaxHandleCountWhenGettingRangeHandleThenUnsupportedFeatureIsReturned) {
    rangeContext->maxIpcRangeHandleCount = 0u;

    ze_ipc_phys_mem_handle_range_ext_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_DESC;
    desc.size = MemoryConstants::pageSize;
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, rangeContext->getIpcMemHandle(reinterpret_cast<void *>(0x100000), &desc, &ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenContiguousDeviceMappingsWhenGettingRangeHandleThenRangeTransportIsCreated) {
    void *basePtr = reinterpret_cast<void *>(0x100000);
    const size_t mappingSize = MemoryConstants::pageSize;
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

    ze_ipc_phys_mem_handle_range_ext_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_DESC;
    desc.size = 2u * mappingSize;
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->getIpcMemHandle(basePtr, &desc, &ipcHandle));
    EXPECT_TRUE(rangeContext->callIsIpcRangeHandle(ipcHandle));

    auto header = reinterpret_cast<const IpcRangeTransportHeader *>(gCommonTransportStorage);
    EXPECT_EQ(ipcRangeHandleMagic, header->magic);
    EXPECT_EQ(2u, header->numHandles);

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->putIpcMemHandle(ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenHostAndDeviceMappingsWhenGettingRangeHandleThenInvalidArgumentIsReturned) {
    void *basePtr = reinterpret_cast<void *>(0x200000);
    const size_t mappingSize = MemoryConstants::pageSize;
    void *secondPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(basePtr) + mappingSize);

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    NEO::MockGraphicsAllocation alloc1(neoDevice->getRootDeviceIndex(), secondPtr, mappingSize);
    alloc1.allocationType = NEO::AllocationType::bufferHostMemory;

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    NEO::MemoryMappedRange range1{};
    range1.ptr = secondPtr;
    range1.size = mappingSize;
    range1.mappedAllocation.allocation = &alloc1;

    installReservation(basePtr, {&range0, &range1});

    ze_ipc_phys_mem_handle_range_ext_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_DESC;
    desc.size = 2u * mappingSize;
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, rangeContext->getIpcMemHandle(basePtr, &desc, &ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenLeadingOffsetWhenOpeningRangeHandleThenMergedPointerIsShifted) {
    const uint64_t leadingOffset = MemoryConstants::pageSize;
    buildTransport(2u, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory), leadingOffset, false);
    auto ipcHandle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport));

    void *ptr = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED, &ptr));
    EXPECT_EQ(reinterpret_cast<uint8_t *>(&rangeMemoryManager->mergedBaseStorage) + leadingOffset, ptr);
    EXPECT_TRUE(rangeMemoryManager->lastUncachedBias);
}

TEST_F(IpcMemHandleRangeCommonTest, givenNoLeadingOffsetWhenOpeningRangeHandleThenMergedBaseIsReturned) {
    buildTransport(2u, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory), 0u, false);
    auto ipcHandle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport));

    void *ptr = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, 0u, &ptr));
    EXPECT_EQ(&rangeMemoryManager->mergedBaseStorage, ptr);
    EXPECT_FALSE(rangeMemoryManager->lastUncachedBias);
}

TEST_F(IpcMemHandleRangeCommonTest, givenChunkTypeNotMatchingRangeClassWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    buildTransport(1u, static_cast<uint8_t>(InternalIpcMemoryType::hostUnifiedMemory), 0u, false);
    auto ipcHandle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport));

    void *ptr = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, rangeContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, 0u, &ptr));
    EXPECT_EQ(1u, rangeContext->releaseImportedCalls);
}

TEST_F(IpcMemHandleRangeCommonTest, givenRangeHandleWhenCallingPutIpcMemHandleThenTrackedTransportIsReleased) {
    auto ipcHandle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport), IpcHandleType::fdHandle, 55u);

    CommonRangeContext::IpcRangeTransportEntry entry;
    entry.baseAddress = reinterpret_cast<void *>(0x300000);
    entry.transportPtr = gCommonTransportStorage;
    entry.transportHandleKey = normalizeIPCHandle(*reinterpret_cast<const IpcOpaqueMemoryData *>(ipcHandle.data));
    entry.handleKeys = {commonChunkHandleBase, commonChunkHandleBase + 1u};
    rangeContext->ipcRangeTransports.push_back(std::move(entry));
    rangeContext->ipcRangeTransportsPresent.store(true, std::memory_order_release);

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->putIpcMemHandle(ipcHandle));
    EXPECT_TRUE(rangeContext->ipcRangeTransports.empty());
    EXPECT_EQ(1u, rangeContext->freeMemCalls);
}

TEST_F(IpcMemHandleRangeCommonTest, givenNonMatchingRangeHandleWhenCallingPutIpcRangeHandleThenNothingIsReleased) {
    auto ipcHandle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport), IpcHandleType::fdHandle, 56u);

    CommonRangeContext::IpcRangeTransportEntry entry;
    entry.baseAddress = reinterpret_cast<void *>(0x300000);
    entry.transportPtr = gCommonTransportStorage;
    entry.transportHandleKey = 999u;
    rangeContext->ipcRangeTransports.push_back(std::move(entry));

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->putIpcMemHandle(ipcHandle));
    EXPECT_EQ(1u, rangeContext->ipcRangeTransports.size());
    EXPECT_EQ(0u, rangeContext->freeMemCalls);
}

TEST_F(IpcMemHandleRangeCommonTest, givenNullTransportPtrWhenReleasingRangeTransportThenFreeIsNotCalled) {
    rangeContext->callReleaseIpcRangeTransport(nullptr);
    EXPECT_EQ(0u, rangeContext->freeMemCalls);
}

TEST_F(IpcMemHandleRangeCommonTest, givenNoTrackedTransportsWhenReleasingTransportForPtrThenFalseIsReturned) {
    EXPECT_FALSE(rangeContext->callReleaseIpcRangeTransportForPtr(reinterpret_cast<void *>(0x300000)));
}

TEST_F(IpcMemHandleRangeCommonTest, givenTrackedTransportsWhenReleasingTransportForPtrThenOnlyMatchingEntryIsReleased) {
    void *matchingBase = reinterpret_cast<void *>(0x300000);

    CommonRangeContext::IpcRangeTransportEntry matching;
    matching.baseAddress = matchingBase;
    matching.transportPtr = gCommonTransportStorage;
    rangeContext->ipcRangeTransports.push_back(std::move(matching));

    CommonRangeContext::IpcRangeTransportEntry other;
    other.baseAddress = reinterpret_cast<void *>(0x400000);
    other.transportPtr = nullptr;
    rangeContext->ipcRangeTransports.push_back(std::move(other));
    rangeContext->ipcRangeTransportsPresent.store(true, std::memory_order_release);

    EXPECT_TRUE(rangeContext->callReleaseIpcRangeTransportForPtr(matchingBase));
    EXPECT_EQ(1u, rangeContext->ipcRangeTransports.size());
    EXPECT_EQ(1u, rangeContext->freeMemCalls);
}

TEST_F(IpcMemHandleRangeCommonTest, whenQueryingRealMaxIpcRangeHandleCountThenExportSupportMatchesTheReportedLimit) {
    const uint32_t maxHandleCount = rangeContext->callRealMaxIpcRangeHandleCount();
    rangeContext->maxIpcRangeHandleCount = maxHandleCount;

    ze_ipc_phys_mem_handle_range_ext_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IPC_PHYS_MEM_HANDLE_RANGE_EXT_DESC;
    desc.size = MemoryConstants::pageSize;
    ze_ipc_mem_handle_t ipcHandle = {};
    auto result = rangeContext->getIpcMemHandle(reinterpret_cast<void *>(0x500000), &desc, &ipcHandle);

    if (maxHandleCount == 0u) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    }
}

TEST_F(IpcMemHandleRangeCommonTest, givenTransportEncodeFailingWhenGettingRangeHandleThenOutOfHostMemoryIsReturned) {
    void *basePtr = reinterpret_cast<void *>(0x600000);
    const size_t mappingSize = MemoryConstants::pageSize;
    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});
    rangeSvmManager->failTransportInternalHandle = true;

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, rangeContext->getIpcMemHandle(basePtr, &desc, &ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenHostMappingsWhenGettingRangeHandleThenTransportHeaderMarksHostRange) {
    void *basePtr = reinterpret_cast<void *>(0x700000);
    const size_t mappingSize = MemoryConstants::pageSize;
    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    alloc0.allocationType = NEO::AllocationType::bufferHostMemory;

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->getIpcMemHandle(basePtr, &desc, &ipcHandle));
    auto header = reinterpret_cast<const IpcRangeTransportHeader *>(gCommonTransportStorage);
    EXPECT_EQ(1u, header->rangeIsHost);

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->putIpcMemHandle(ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenNonReservedPtrWithFittingAllocationWhenGettingRangeHandleThenSingleHandleExportIsUsed) {
    void *ptr = reinterpret_cast<void *>(0x800000);
    const size_t allocSize = 2u * MemoryConstants::pageSize;
    installSvmAllocation(ptr, allocSize);

    auto desc = buildRangeDesc(MemoryConstants::pageSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->getIpcMemHandle(ptr, &desc, &ipcHandle));
    EXPECT_FALSE(rangeContext->callIsIpcRangeHandle(ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenNonReservedPtrWithSizeExceedingAllocationWhenGettingRangeHandleThenInvalidArgumentIsReturned) {
    void *ptr = reinterpret_cast<void *>(0x900000);
    const size_t allocSize = MemoryConstants::pageSize;
    installSvmAllocation(ptr, allocSize);

    auto desc = buildRangeDesc(4u * MemoryConstants::pageSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, rangeContext->getIpcMemHandle(ptr, &desc, &ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenHostRangeWithDeviceChunkWhenOpeningRangeHandleThenInvalidArgumentIsReturned) {
    buildTransport(1u, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory), 0u, true);
    auto ipcHandle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport));

    void *ptr = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, rangeContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, 0u, &ptr));
}

TEST_F(IpcMemHandleRangeCommonTest, givenDeviceImportReturningNullWhenOpeningRangeHandleThenOutOfDeviceMemoryIsReturned) {
    rangeMemoryManager->failImportFdHandles = true;
    buildTransport(2u, static_cast<uint8_t>(InternalIpcMemoryType::deviceUnifiedMemory), 0u, false);
    auto ipcHandle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport));

    void *ptr = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, rangeContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, 0u, &ptr));
}

TEST_F(IpcMemHandleRangeCommonTest, givenRangeSizeOverflowingAddressSpaceWhenGettingRangeHandleThenAddressNotFoundIsReturned) {
    void *basePtr = reinterpret_cast<void *>(0xFFFFFFFFFFFF0000ull);
    const size_t mappingSize = MemoryConstants::pageSize;
    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(0x20000u);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_ADDRESS_NOT_FOUND, rangeContext->getIpcMemHandle(basePtr, &desc, &ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenPooledAllocationWithRangeSizeFittingPooledSizeWhenGettingRangeHandleThenSingleHandleExportIsUsed) {
    void *poolBase = reinterpret_cast<void *>(0xA00000);
    const size_t poolSize = 8u * MemoryConstants::pageSize;
    const size_t pooledSize = 2u * MemoryConstants::pageSize;
    void *ptr = reinterpret_cast<uint8_t *>(poolBase) + MemoryConstants::pageSize;

    auto pool = installPooledDeviceAllocation(poolBase, poolSize, ptr, pooledSize);
    ASSERT_EQ(pool, neoDevice->getUsmPoolOwningPtr(ptr));

    auto desc = buildRangeDesc(pooledSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->getIpcMemHandle(ptr, &desc, &ipcHandle));
    EXPECT_FALSE(rangeContext->callIsIpcRangeHandle(ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenPooledAllocationWithRangeSizeExceedingPooledSizeWhenGettingRangeHandleThenInvalidArgumentIsReturned) {
    void *poolBase = reinterpret_cast<void *>(0xB00000);
    const size_t poolSize = 8u * MemoryConstants::pageSize;
    const size_t pooledSize = 2u * MemoryConstants::pageSize;
    void *ptr = reinterpret_cast<uint8_t *>(poolBase) + MemoryConstants::pageSize;

    auto pool = installPooledDeviceAllocation(poolBase, poolSize, ptr, pooledSize);
    ASSERT_EQ(pool, neoDevice->getUsmPoolOwningPtr(ptr));

    auto desc = buildRangeDesc(4u * MemoryConstants::pageSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, rangeContext->getIpcMemHandle(ptr, &desc, &ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenNonReservedPtrWithoutAllocationWhenGettingRangeHandleThenInvalidArgumentIsReturned) {
    uint64_t unknownStorage = 0u;

    auto desc = buildRangeDesc(MemoryConstants::pageSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, rangeContext->getIpcMemHandle(&unknownStorage, &desc, &ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenFabricAccessibleRangeDescWhenGettingRangeHandleThenChunkHandlesCarryReservedHandleData) {
    void *basePtr = reinterpret_cast<void *>(0xC00000);
    const size_t mappingSize = MemoryConstants::pageSize;

    ReservedDataRecordingRangeAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});

    ze_ipc_mem_handle_type_ext_desc_t typeDesc = {};
    typeDesc.stype = ZE_STRUCTURE_TYPE_IPC_MEM_HANDLE_TYPE_EXT_DESC;
    typeDesc.typeFlags = ZE_IPC_MEM_HANDLE_TYPE_FLAG_FABRIC_ACCESSIBLE;

    auto desc = buildRangeDesc(mappingSize);
    desc.pNext = &typeDesc;
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->getIpcMemHandle(basePtr, &desc, &ipcHandle));
    EXPECT_TRUE(alloc0.reservedHandleDataWasSet);
    EXPECT_TRUE(rangeContext->callIsIpcRangeHandle(ipcHandle));

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->putIpcMemHandle(ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenHostRangeWithFailingHostMergeWhenOpeningRangeHandleThenOutOfHostMemoryIsReturned) {
    rangeMemoryManager->failCreateHostAllocation = true;
    buildTransport(2u, static_cast<uint8_t>(InternalIpcMemoryType::hostUnifiedMemory), 0u, true);
    auto ipcHandle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport));

    void *ptr = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, rangeContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, 0u, &ptr));
    EXPECT_TRUE(rangeMemoryManager->createHostAllocationCalled);
    EXPECT_FALSE(rangeMemoryManager->lastUncachedBias);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(IpcMemHandleRangeCommonTest, givenHostRangeWithSucceedingHostMergeWhenOpeningRangeHandleThenHostSvmAllocationIsInserted) {
    const uint32_t numHandles = 2u;
    buildTransport(numHandles, static_cast<uint8_t>(InternalIpcMemoryType::hostUnifiedMemory), 0u, true);
    auto ipcHandle = buildHandle(static_cast<uint8_t>(InternalIpcMemoryType::ipcRangeTransport));

    void *ptr = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->callOpenIpcRangeHandle(device->toHandle(), ipcHandle, 0u, &ptr));
    ASSERT_NE(nullptr, ptr);
    EXPECT_TRUE(rangeMemoryManager->createHostAllocationCalled);
    EXPECT_EQ(numHandles, rangeMemoryManager->lastHostPhysicalOffsets.size());

    auto allocData = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, allocData->memoryType);

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->closeIpcMemHandle(ptr));
}

TEST_F(IpcMemHandleRangeCommonTest, givenHoleBetweenReservationMappingsWhenGettingRangeHandleThenAddressNotFoundIsReturned) {
    void *basePtr = reinterpret_cast<void *>(0xD00000);
    const size_t mappingSize = MemoryConstants::pageSize;
    void *thirdPtr = reinterpret_cast<uint8_t *>(basePtr) + 2u * mappingSize;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);
    NEO::MockGraphicsAllocation alloc2(neoDevice->getRootDeviceIndex(), thirdPtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    NEO::MemoryMappedRange range2{};
    range2.ptr = thirdPtr;
    range2.size = mappingSize;
    range2.mappedAllocation.allocation = &alloc2;

    installReservation(basePtr, {&range0, &range2});

    auto desc = buildRangeDesc(3u * mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_ADDRESS_NOT_FOUND, rangeContext->getIpcMemHandle(basePtr, &desc, &ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenMappingEndingBeforeRangeStartWhenGettingRangeHandleThenEarlierMappingIsSkippedAndRangeIsExported) {
    void *reservationBase = reinterpret_cast<void *>(0xE00000);
    const size_t mappingSize = MemoryConstants::pageSize;
    void *secondPtr = reinterpret_cast<uint8_t *>(reservationBase) + mappingSize;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), reservationBase, mappingSize);
    NEO::MockGraphicsAllocation alloc1(neoDevice->getRootDeviceIndex(), secondPtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = reservationBase;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    NEO::MemoryMappedRange range1{};
    range1.ptr = secondPtr;
    range1.size = mappingSize;
    range1.mappedAllocation.allocation = &alloc1;

    installReservation(reservationBase, {&range0, &range1});
    reservation->virtualAddressRange.address = reinterpret_cast<uint64_t>(reservationBase);
    reservation->virtualAddressRange.size = 0x10000u;

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->getIpcMemHandle(secondPtr, &desc, &ipcHandle));
    EXPECT_TRUE(rangeContext->callIsIpcRangeHandle(ipcHandle));

    auto header = reinterpret_cast<const IpcRangeTransportHeader *>(gCommonTransportStorage);
    EXPECT_EQ(1u, header->numHandles);
    EXPECT_EQ(0u, header->leadingOffset);

    EXPECT_EQ(ZE_RESULT_SUCCESS, rangeContext->putIpcMemHandle(ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenMappingWithoutBackingAllocationWhenGettingRangeHandleThenAddressNotFoundIsReturned) {
    void *basePtr = reinterpret_cast<void *>(0xF00000);
    const size_t mappingSize = MemoryConstants::pageSize;

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = nullptr;

    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_ADDRESS_NOT_FOUND, rangeContext->getIpcMemHandle(basePtr, &desc, &ipcHandle));
}

TEST_F(IpcMemHandleRangeCommonTest, givenRangeExtendingPastLastMappingWhenGettingRangeHandleThenAddressNotFoundIsReturned) {
    void *basePtr = reinterpret_cast<void *>(0x1000000);
    const size_t mappingSize = MemoryConstants::pageSize;

    NEO::MockGraphicsAllocation alloc0(neoDevice->getRootDeviceIndex(), basePtr, mappingSize);

    NEO::MemoryMappedRange range0{};
    range0.ptr = basePtr;
    range0.size = mappingSize;
    range0.mappedAllocation.allocation = &alloc0;

    installReservation(basePtr, {&range0});

    auto desc = buildRangeDesc(2u * mappingSize);
    ze_ipc_mem_handle_t ipcHandle = {};

    EXPECT_EQ(ZE_RESULT_ERROR_ADDRESS_NOT_FOUND, rangeContext->getIpcMemHandle(basePtr, &desc, &ipcHandle));
}

} // namespace ult
} // namespace L0

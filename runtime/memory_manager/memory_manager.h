/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/host_ptr_defines.h"
#include "runtime/os_interface/32bit_memory.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace OCLRT {
class AllocationsList;
class Device;
class DeferredDeleter;
class ExecutionEnvironment;
class GraphicsAllocation;
class HostPtrManager;
class CommandStreamReceiver;
class OsContext;
class TimestampPacket;

struct HwPerfCounter;
struct HwTimeStamps;

template <typename T1>
class TagAllocator;

template <typename T1>
struct TagNode;

class AllocsTracker;
class MapBaseAllocationTracker;
class SVMAllocsManager;

enum AllocationUsage {
    TEMPORARY_ALLOCATION,
    REUSABLE_ALLOCATION
};

enum AllocationOrigin {
    EXTERNAL_ALLOCATION,
    INTERNAL_ALLOCATION
};

struct AllocationFlags {
    union {
        struct {
            uint32_t allocateMemory : 1;
            uint32_t flushL3RequiredForRead : 1;
            uint32_t flushL3RequiredForWrite : 1;
            uint32_t reserved : 29;
        } flags;
        uint32_t allFlags;
    };

    static_assert(sizeof(AllocationFlags::flags) == sizeof(AllocationFlags::allFlags), "");

    AllocationFlags() {
        allFlags = 0;
        flags.flushL3RequiredForRead = 1;
        flags.flushL3RequiredForWrite = 1;
    }

    AllocationFlags(bool allocateMemory) {
        allFlags = 0;
        flags.flushL3RequiredForRead = 1;
        flags.flushL3RequiredForWrite = 1;
        flags.allocateMemory = allocateMemory;
    }
};

struct AllocationData {
    union {
        struct {
            uint32_t mustBeZeroCopy : 1;
            uint32_t allocateMemory : 1;
            uint32_t allow64kbPages : 1;
            uint32_t allow32Bit : 1;
            uint32_t useSystemMemory : 1;
            uint32_t forcePin : 1;
            uint32_t uncacheable : 1;
            uint32_t flushL3 : 1;
            uint32_t reserved : 24;
        } flags;
        uint32_t allFlags = 0;
    };
    static_assert(sizeof(AllocationData::flags) == sizeof(AllocationData::allFlags), "");

    GraphicsAllocation::AllocationType type = GraphicsAllocation::AllocationType::UNKNOWN;
    const void *hostPtr = nullptr;
    size_t size = 0;
    DevicesBitfield devicesBitfield = 0;
};

struct AlignedMallocRestrictions {
    uintptr_t minAddress;
};

constexpr size_t paddingBufferSize = 2 * MemoryConstants::megaByte;

class Gmm;
struct ImageInfo;

class MemoryManager {
  public:
    enum AllocationStatus {
        Success = 0,
        Error,
        InvalidHostPointer,
        RetryInNonDevicePool
    };

    MemoryManager(bool enable64kbpages, bool enableLocalMemory, ExecutionEnvironment &executionEnvironment);

    virtual ~MemoryManager();
    MOCKABLE_VIRTUAL void *allocateSystemMemory(size_t size, size_t alignment);

    virtual void addAllocationToHostPtrManager(GraphicsAllocation *memory) = 0;
    virtual void removeAllocationFromHostPtrManager(GraphicsAllocation *memory) = 0;

    GraphicsAllocation *allocateGraphicsMemory(size_t size) {
        return allocateGraphicsMemory(size, MemoryConstants::preferredAlignment, false, false);
    }

    virtual GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) = 0;

    virtual GraphicsAllocation *allocateGraphicsMemory64kb(size_t size, size_t alignment, bool forcePin, bool preferRenderCompressed) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(size_t size, void *cpuPtr) = 0;

    virtual GraphicsAllocation *allocateGraphicsMemory(size_t size, const void *ptr) {
        return MemoryManager::allocateGraphicsMemory(size, ptr, false);
    }
    virtual GraphicsAllocation *allocateGraphicsMemory(size_t size, const void *ptr, bool forcePin);

    GraphicsAllocation *allocateGraphicsMemoryForHostPtr(size_t size, void *ptr, bool fullRangeSvm, bool requiresL3Flush) {
        if (fullRangeSvm) {
            return allocateGraphicsMemory(size, ptr);
        } else {
            auto allocation = allocateGraphicsMemoryForNonSvmHostPtr(size, ptr);
            if (allocation) {
                allocation->flushL3Required = requiresL3Flush;
            }
            return allocation;
        }
    }

    virtual GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) = 0;

    virtual GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) = 0;

    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryInPreferredPool(AllocationFlags flags, DevicesBitfield devicesBitfield, const void *hostPtr, size_t size, GraphicsAllocation::AllocationType type);

    GraphicsAllocation *allocateGraphicsMemoryForSVM(size_t size, bool coherent);

    virtual GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) = 0;

    virtual GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) = 0;

    virtual GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
        status = AllocationStatus::Error;
        if (!allocationData.flags.useSystemMemory && !(allocationData.flags.allow32Bit && this->force32bitAllocations)) {
            auto allocation = allocateGraphicsMemory(allocationData);
            if (allocation) {
                allocation->devicesBitfield = allocationData.devicesBitfield;
                allocation->flushL3Required = allocationData.flags.flushL3;
                status = AllocationStatus::Success;
            }
            return allocation;
        }
        status = AllocationStatus::RetryInNonDevicePool;
        return nullptr;
    }

    virtual bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) { return false; };

    virtual void *lockResource(GraphicsAllocation *graphicsAllocation) = 0;
    virtual void unlockResource(GraphicsAllocation *graphicsAllocation) = 0;

    void cleanGraphicsMemoryCreatedFromHostPtr(GraphicsAllocation *);
    GraphicsAllocation *createGraphicsAllocationWithPadding(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding);
    virtual GraphicsAllocation *createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding);

    virtual AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) = 0;
    virtual void cleanOsHandles(OsHandleStorage &handleStorage) = 0;

    void freeSystemMemory(void *ptr);

    virtual void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) = 0;

    void freeGraphicsMemory(GraphicsAllocation *gfxAllocation);

    void checkGpuUsageAndDestroyGraphicsAllocations(GraphicsAllocation *gfxAllocation);

    virtual uint64_t getSystemSharedMemory() = 0;

    virtual uint64_t getMaxApplicationAddress() = 0;

    virtual uint64_t getInternalHeapBaseAddress() = 0;

    TagAllocator<HwTimeStamps> *getEventTsAllocator();
    TagAllocator<HwPerfCounter> *getEventPerfCountAllocator();
    TagAllocator<TimestampPacket> *getTimestampPacketAllocator();

    virtual GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, size_t hostPtrSize, const void *hostPtr) = 0;

    bool peek64kbPagesEnabled() const { return enable64kbpages; }
    bool peekForce32BitAllocations() { return force32bitAllocations; }
    void setForce32BitAllocations(bool newValue);

    std::unique_ptr<Allocator32bit> allocator32Bit;

    bool peekVirtualPaddingSupport() { return virtualPaddingAvailable; }
    void setVirtualPaddingSupport(bool virtualPaddingSupport) { virtualPaddingAvailable = virtualPaddingSupport; }
    GraphicsAllocation *peekPaddingAllocation() { return paddingAllocation; }

    DeferredDeleter *getDeferredDeleter() const {
        return deferredDeleter.get();
    }

    void waitForDeletions();

    bool isAsyncDeleterEnabled() const;
    bool isLocalMemorySupported() const;
    virtual bool isMemoryBudgetExhausted() const;

    virtual AlignedMallocRestrictions *getAlignedMallocRestrictions() {
        return nullptr;
    }

    MOCKABLE_VIRTUAL void *alignedMallocWrapper(size_t bytes, size_t alignment) {
        return ::alignedMalloc(bytes, alignment);
    }

    MOCKABLE_VIRTUAL void alignedFreeWrapper(void *ptr) {
        ::alignedFree(ptr);
    }

    void registerOsContext(OsContext *contextToRegister);
    size_t getOsContextCount() { return registeredOsContexts.size(); }
    CommandStreamReceiver *getCommandStreamReceiver(uint32_t contextId);
    HostPtrManager *getHostPtrManager() const { return hostPtrManager.get(); }

  protected:
    static bool getAllocationData(AllocationData &allocationData, const AllocationFlags &flags, const DevicesBitfield devicesBitfield,
                                  const void *hostPtr, size_t size, GraphicsAllocation::AllocationType type);

    GraphicsAllocation *allocateGraphicsMemory(const AllocationData &allocationData);
    std::unique_ptr<TagAllocator<HwTimeStamps>> profilingTimeStampAllocator;
    std::unique_ptr<TagAllocator<HwPerfCounter>> perfCounterAllocator;
    std::unique_ptr<TagAllocator<TimestampPacket>> timestampPacketAllocator;
    bool force32bitAllocations = false;
    bool virtualPaddingAvailable = false;
    GraphicsAllocation *paddingAllocation = nullptr;
    void applyCommonCleanup();
    std::unique_ptr<DeferredDeleter> deferredDeleter;
    bool asyncDeleterEnabled = false;
    bool enable64kbpages = false;
    bool localMemorySupported = false;
    ExecutionEnvironment &executionEnvironment;
    std::vector<OsContext *> registeredOsContexts;
    std::unique_ptr<HostPtrManager> hostPtrManager;
};

std::unique_ptr<DeferredDeleter> createDeferredDeleter();
} // namespace OCLRT

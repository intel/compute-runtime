/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "public/cl_ext_private.h"
#include "runtime/command_stream/preemption_mode.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/host_ptr_defines.h"
#include "runtime/os_interface/32bit_memory.h"
#include "engine_node.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace OCLRT {
class CommandStreamReceiver;
class DeferredDeleter;
class ExecutionEnvironment;
class Gmm;
class GraphicsAllocation;
class HostPtrManager;
class OsContext;
struct ImageInfo;

using CsrContainer = std::vector<std::vector<std::unique_ptr<CommandStreamReceiver>>>;

enum AllocationUsage {
    TEMPORARY_ALLOCATION,
    REUSABLE_ALLOCATION
};

struct AllocationProperties {
    union {
        struct {
            uint32_t allocateMemory : 1;
            uint32_t flushL3RequiredForRead : 1;
            uint32_t flushL3RequiredForWrite : 1;
            uint32_t forcePin : 1;
            uint32_t uncacheable : 1;
            uint32_t multiOsContextCapable : 1;
            uint32_t reserved : 26;
        } flags;
        uint32_t allFlags = 0;
    };
    static_assert(sizeof(AllocationProperties::flags) == sizeof(AllocationProperties::allFlags), "");
    size_t size = 0;
    size_t alignment = 0;
    GraphicsAllocation::AllocationType allocationType = GraphicsAllocation::AllocationType::UNKNOWN;
    ImageInfo *imgInfo = nullptr;

    AllocationProperties(size_t size, GraphicsAllocation::AllocationType allocationType)
        : AllocationProperties(true, size, allocationType) {}
    AllocationProperties(bool allocateMemory, size_t size, GraphicsAllocation::AllocationType allocationType)
        : size(size), allocationType(allocationType) {
        allFlags = 0;
        flags.flushL3RequiredForRead = 1;
        flags.flushL3RequiredForWrite = 1;
        flags.allocateMemory = allocateMemory;
    }
    AllocationProperties(ImageInfo *imgInfo, bool allocateMemory) : AllocationProperties(allocateMemory, 0, GraphicsAllocation::AllocationType::IMAGE) {
        this->imgInfo = imgInfo;
    }
};

struct AlignedMallocRestrictions {
    uintptr_t minAddress;
};

constexpr size_t paddingBufferSize = 2 * MemoryConstants::megaByte;

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

    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) {
        return allocateGraphicsMemoryInPreferredPool(properties, 0u, nullptr);
    }

    virtual GraphicsAllocation *allocateGraphicsMemory(const AllocationProperties &properties, const void *ptr) {
        return allocateGraphicsMemoryInPreferredPool(properties, 0u, ptr);
    }

    GraphicsAllocation *allocateGraphicsMemoryForHostPtr(size_t size, void *ptr, bool fullRangeSvm, bool requiresL3Flush) {
        if (fullRangeSvm && DebugManager.flags.EnableHostPtrTracking.get()) {
            return allocateGraphicsMemory({false, size, GraphicsAllocation::AllocationType::UNDECIDED}, ptr);
        } else {
            auto allocation = allocateGraphicsMemoryForNonSvmHostPtr(size, ptr);
            if (allocation) {
                allocation->flushL3Required = requiresL3Flush;
            }
            return allocation;
        }
    }

    GraphicsAllocation *allocateGraphicsMemoryInPreferredPool(AllocationProperties properties, DevicesBitfield devicesBitfield, const void *hostPtr);

    virtual GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) = 0;

    virtual GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) = 0;

    virtual bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) { return false; };

    void *lockResource(GraphicsAllocation *graphicsAllocation);
    void unlockResource(GraphicsAllocation *graphicsAllocation);

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

    OsContext *createAndRegisterOsContext(EngineInstanceT engineType, uint32_t numSupportedDevices, PreemptionMode preemptionMode);
    uint32_t getOsContextCount() { return static_cast<uint32_t>(registeredOsContexts.size()); }
    CommandStreamReceiver *getDefaultCommandStreamReceiver(uint32_t deviceId) const;
    const CsrContainer &getCommandStreamReceivers() const;
    HostPtrManager *getHostPtrManager() const { return hostPtrManager.get(); }
    void setDefaultEngineIndex(uint32_t index) { defaultEngineIndex = index; }

  protected:
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
                uint32_t preferRenderCompressed : 1;
                uint32_t multiOsContextCapable : 1;
                uint32_t requiresCpuAccess : 1;
                uint32_t reserved : 21;
            } flags;
            uint32_t allFlags = 0;
        };
        static_assert(sizeof(AllocationData::flags) == sizeof(AllocationData::allFlags), "");
        GraphicsAllocation::AllocationType type = GraphicsAllocation::AllocationType::UNKNOWN;
        AllocationOrigin allocationOrigin = AllocationOrigin::EXTERNAL_ALLOCATION;
        const void *hostPtr = nullptr;
        size_t size = 0;
        size_t alignment = 0;
        DevicesBitfield devicesBitfield = 0;
        ImageInfo *imgInfo = nullptr;
    };

    static bool getAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const DevicesBitfield devicesBitfield,
                                  const void *hostPtr);

    virtual GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(size_t size, void *cpuPtr) = 0;
    GraphicsAllocation *allocateGraphicsMemory(const AllocationData &allocationData);
    virtual GraphicsAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData);
    virtual GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemory64kb(AllocationData allocationData) = 0;
    virtual GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
        status = AllocationStatus::Error;
        switch (allocationData.type) {
        case GraphicsAllocation::AllocationType::IMAGE:
        case GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY:
            break;
        default:
            if (!allocationData.flags.useSystemMemory && !(allocationData.flags.allow32Bit && this->force32bitAllocations)) {
                auto allocation = allocateGraphicsMemory(allocationData);
                if (allocation) {
                    allocation->devicesBitfield = allocationData.devicesBitfield;
                    allocation->flushL3Required = allocationData.flags.flushL3;
                    status = AllocationStatus::Success;
                }
                return allocation;
            }
        }
        status = AllocationStatus::RetryInNonDevicePool;
        return nullptr;
    }
    GraphicsAllocation *allocateGraphicsMemoryForImageFromHostPtr(const AllocationData &allocationData);
    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData);
    virtual GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) = 0;

    virtual void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) = 0;
    virtual void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) = 0;

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
    uint32_t latestContextId = std::numeric_limits<uint32_t>::max();
    uint32_t defaultEngineIndex = 0;
    std::unique_ptr<DeferredDeleter> multiContextResourceDestructor;
};

std::unique_ptr<DeferredDeleter> createDeferredDeleter();
} // namespace OCLRT

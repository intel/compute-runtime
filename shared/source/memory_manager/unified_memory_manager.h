/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/memadvise_flags.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/sorted_vector.h"
#include "shared/source/utilities/spinlock.h"

#include "memory_properties_flags.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <shared_mutex>
#include <type_traits>

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
class MemoryManager;
class Device;
struct VirtualMemoryReservation;

struct SvmAllocationData : NEO::NonCopyableAndNonMovableClass {
    SvmAllocationData(uint32_t maxRootDeviceIndex) : gpuAllocations(maxRootDeviceIndex), maxRootDeviceIndex(maxRootDeviceIndex){};
    SvmAllocationData(const SvmAllocationData &svmAllocData) : SvmAllocationData(svmAllocData.maxRootDeviceIndex) {
        this->allocationFlagsProperty = svmAllocData.allocationFlagsProperty;
        this->cpuAllocation = svmAllocData.cpuAllocation;
        this->device = svmAllocData.device;
        this->size = svmAllocData.size;
        this->memoryType = svmAllocData.memoryType;
        this->allocId = svmAllocData.allocId;
        this->pageSizeForAlignment = svmAllocData.pageSizeForAlignment;
        this->isImportedAllocation = svmAllocData.isImportedAllocation;
        this->isInternalAllocation = svmAllocData.isInternalAllocation;
        for (auto allocation : svmAllocData.gpuAllocations.getGraphicsAllocations()) {
            if (allocation) {
                this->gpuAllocations.addAllocation(allocation);
            }
        }
        this->mappedAllocData = svmAllocData.mappedAllocData;
        this->virtualReservationData = svmAllocData.virtualReservationData;
    }
    SvmAllocationData(SvmAllocationData &&other) noexcept = delete;
    GraphicsAllocation *cpuAllocation = nullptr;
    MultiGraphicsAllocation gpuAllocations;
    VirtualMemoryReservation *virtualReservationData = nullptr;
    size_t size = 0;
    size_t pageSizeForAlignment = 0;
    InternalMemoryType memoryType = InternalMemoryType::svm;
    MemoryProperties allocationFlagsProperty;
    Device *device = nullptr;
    bool isImportedAllocation = false;
    void setAllocId(uint32_t id) {
        allocId = id;
    }
    bool mappedAllocData = false;
    bool isInternalAllocation = false;
    bool isSavedForReuse = false;

    uint32_t getAllocId() const {
        return allocId;
    }

    static constexpr uint32_t uninitializedAllocId = std::numeric_limits<uint32_t>::max();

  protected:
    const uint32_t maxRootDeviceIndex;
    uint32_t allocId = uninitializedAllocId;
};

static_assert(NEO::NonMovable<SvmAllocationData>);

struct SvmMapOperation {
    void *regionSvmPtr = nullptr;
    size_t regionSize = 0;
    void *baseSvmPtr = nullptr;
    size_t offset = 0;
    bool readOnlyMap = false;
};

class SVMAllocsManager {
  public:
    using SortedVectorBasedAllocationTracker = BaseSortedPointerWithValueVector<SvmAllocationData>;

    class MapBasedAllocationTracker {
        friend class SVMAllocsManager;

      public:
        using SvmAllocationContainer = std::map<const void *, SvmAllocationData>;
        void insert(const SvmAllocationData &);
        void remove(const SvmAllocationData &);
        SvmAllocationData *get(const void *);
        size_t getNumAllocs() const { return allocations.size(); };

        void freeAllocations(NEO::MemoryManager &memoryManager);

        SvmAllocationContainer allocations;
        NEO::SpinLock mutex;
    };

    struct MapOperationsTracker {
        using SvmMapOperationsContainer = std::map<const void *, SvmMapOperation>;
        void insert(SvmMapOperation);
        void remove(const void *);
        SvmMapOperation *get(const void *);
        size_t getNumMapOperations() const { return operations.size(); };

      protected:
        SvmMapOperationsContainer operations;
    };

    struct SvmAllocationProperties {
        bool coherent = false;
        bool hostPtrReadOnly = false;
        bool readOnly = false;
    };

    struct InternalAllocationsTracker {
        TaskCountType latestSentTaskCount = 0lu;
        TaskCountType latestResidentObjectId = 0lu;
    };

    struct UnifiedMemoryProperties {
        UnifiedMemoryProperties(InternalMemoryType memoryType,
                                size_t alignment,
                                const RootDeviceIndicesContainer &rootDeviceIndices,
                                const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields) : memoryType(memoryType),
                                                                                                alignment(alignment),
                                                                                                rootDeviceIndices(rootDeviceIndices),
                                                                                                subdeviceBitfields(subdeviceBitfields){};
        uint32_t getRootDeviceIndex() const;
        InternalMemoryType memoryType = InternalMemoryType::notSpecified;
        MemoryProperties allocationFlags;
        Device *device = nullptr;
        size_t alignment;
        const RootDeviceIndicesContainer &rootDeviceIndices;
        const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields;
        AllocationType requestedAllocationType = AllocationType::unknown;
        bool isInternalAllocation = false;
    };

    struct SvmCacheAllocationInfo {
        size_t allocationSize;
        void *allocation;
        SvmAllocationData *svmData;
        std::chrono::high_resolution_clock::time_point saveTime;
        bool completed;
        SvmCacheAllocationInfo(size_t allocationSize, void *allocation, SvmAllocationData *svmData, bool completed) : allocationSize(allocationSize), allocation(allocation), svmData(svmData), completed(completed) {
            saveTime = std::chrono::high_resolution_clock::now();
        }
        bool operator<(SvmCacheAllocationInfo const &other) const {
            return allocationSize < other.allocationSize;
        }
        bool operator<(size_t const &size) const {
            return allocationSize < size;
        }
        void markForDelete() {
            allocationSize = 0u;
        }
        static bool isMarkedForDelete(SvmCacheAllocationInfo const &info) {
            return 0 == info.allocationSize;
        }
    };

    struct SvmAllocationCache {
        enum class CacheOperationType {
            insert,
            get,
            trim,
            trimOld
        };

        struct SvmAllocationCachePerfInfo {
            uint64_t allocationSize;
            std::chrono::high_resolution_clock::time_point timePoint;
            InternalMemoryType allocationType;
            CacheOperationType operationType;
            bool isSuccess;
        };

        static constexpr size_t maxServicedSize = 256 * MemoryConstants::megaByte;
        static constexpr size_t minimalSizeToCheckUtilization = 4 * MemoryConstants::pageSize64k;
        static constexpr double minimalAllocUtilization = 0.5;

        SvmAllocationCache();

        static bool sizeAllowed(size_t size) { return size <= SvmAllocationCache::maxServicedSize; }
        bool insert(size_t size, void *ptr, SvmAllocationData *svmData, bool waitForCompletion);
        static bool allocUtilizationAllows(size_t requestedSize, size_t reuseCandidateSize);
        static bool alignmentAllows(void *ptr, size_t alignment);
        bool isInUse(SvmCacheAllocationInfo &cacheAllocInfo);
        void *get(size_t size, const UnifiedMemoryProperties &unifiedMemoryProperties);
        void trim();
        void trimOldAllocs(std::chrono::high_resolution_clock::time_point trimTimePoint, bool trimAll);
        void cleanup();
        void logCacheOperation(const SvmAllocationCachePerfInfo &cachePerfEvent) const;

        std::vector<SvmCacheAllocationInfo> allocations;

        std::mutex mtx;
        SVMAllocsManager *svmAllocsManager = nullptr;
        MemoryManager *memoryManager = nullptr;
        bool enablePerformanceLogging = false;
    };

    enum class FreePolicyType : uint32_t {
        none = 0,
        blocking = 1,
        defer = 2
    };

    SVMAllocsManager(MemoryManager *memoryManager);
    MOCKABLE_VIRTUAL ~SVMAllocsManager();
    void *createSVMAlloc(size_t size,
                         const SvmAllocationProperties svmProperties,
                         const RootDeviceIndicesContainer &rootDeviceIndices,
                         const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields);
    MOCKABLE_VIRTUAL void *createHostUnifiedMemoryAllocation(size_t size,
                                                             const UnifiedMemoryProperties &svmProperties);
    MOCKABLE_VIRTUAL void *createUnifiedMemoryAllocation(size_t size,
                                                         const UnifiedMemoryProperties &svmProperties);
    MOCKABLE_VIRTUAL void *createSharedUnifiedMemoryAllocation(size_t size,
                                                               const UnifiedMemoryProperties &svmProperties,
                                                               void *cmdQ);
    void *createUnifiedKmdMigratedAllocation(size_t size,
                                             const SvmAllocationProperties &svmProperties,
                                             const UnifiedMemoryProperties &unifiedMemoryProperties);

    void setUnifiedAllocationProperties(GraphicsAllocation *allocation, const SvmAllocationProperties &svmProperties);

    template <typename T,
              std::enable_if_t<std::is_same_v<T, void> || std::is_same_v<T, const void>, int> = 0>
    SvmAllocationData *getSVMAlloc(T *ptr) {
        std::shared_lock<std::shared_mutex> lock(mtx);
        return svmAllocs.get(ptr);
    }

    MOCKABLE_VIRTUAL bool freeSVMAlloc(void *ptr, bool blocking);
    MOCKABLE_VIRTUAL bool freeSVMAllocDefer(void *ptr);
    MOCKABLE_VIRTUAL void freeSVMAllocDeferImpl();
    MOCKABLE_VIRTUAL void freeSVMAllocImpl(void *ptr, FreePolicyType policy, SvmAllocationData *svmData);
    bool freeSVMAlloc(void *ptr) { return freeSVMAlloc(ptr, false); }
    void cleanupUSMAllocCaches();
    void trimUSMDeviceAllocCache();
    void trimUSMHostAllocCache();
    void insertSVMAlloc(const SvmAllocationData &svmData);
    void removeSVMAlloc(const SvmAllocationData &svmData);
    size_t getNumAllocs() const { return svmAllocs.getNumAllocs(); }
    MOCKABLE_VIRTUAL size_t getNumDeferFreeAllocs() const { return svmDeferFreeAllocs.getNumAllocs(); }
    SortedVectorBasedAllocationTracker *getSVMAllocs() { return &svmAllocs; }

    MOCKABLE_VIRTUAL void insertSvmMapOperation(void *regionSvmPtr, size_t regionSize, void *baseSvmPtr, size_t offset, bool readOnlyMap);
    void removeSvmMapOperation(const void *regionSvmPtr);
    SvmMapOperation *getSvmMapOperation(const void *regionPtr);
    MOCKABLE_VIRTUAL void addInternalAllocationsToResidencyContainer(uint32_t rootDeviceIndex,
                                                                     ResidencyContainer &residencyContainer,
                                                                     uint32_t requestedTypesMask);
    void makeInternalAllocationsResident(CommandStreamReceiver &commandStreamReceiver, uint32_t requestedTypesMask);
    void *createUnifiedAllocationWithDeviceStorage(size_t size, const SvmAllocationProperties &svmProperties, const UnifiedMemoryProperties &unifiedMemoryProperties);
    void freeSvmAllocationWithDeviceStorage(SvmAllocationData *svmData);
    bool hasHostAllocations();
    std::atomic<uint32_t> allocationsCounter = 0;
    MOCKABLE_VIRTUAL void makeIndirectAllocationsResident(CommandStreamReceiver &commandStreamReceiver, TaskCountType taskCount);
    void prepareIndirectAllocationForDestruction(SvmAllocationData *allocationData, bool isNonBlockingFree);
    void sharedSystemMemAdvise(Device &device, MemAdvise memAdviseOp, const void *ptr, const size_t size);
    MOCKABLE_VIRTUAL void prefetchMemory(Device &device, CommandStreamReceiver &commandStreamReceiver, const void *ptr, const size_t size);
    void prefetchSVMAllocs(Device &device, CommandStreamReceiver &commandStreamReceiver);
    void sharedSystemAtomicAccess(Device &device, AtomicAccessMode mode, const void *ptr, const size_t size);
    std::unique_lock<std::mutex> obtainOwnership();

    std::map<CommandStreamReceiver *, InternalAllocationsTracker> indirectAllocationsResidency;

    using NonGpuDomainAllocsContainer = std::vector<void *>;
    NonGpuDomainAllocsContainer nonGpuDomainAllocs;

    void initUsmAllocationsCaches(Device &device);

    bool submitIndirectAllocationsAsPack(CommandStreamReceiver &csr);

    void waitForEnginesCompletion(SvmAllocationData *allocationData);

  protected:
    void *createZeroCopySvmAllocation(size_t size, const SvmAllocationProperties &svmProperties,
                                      const RootDeviceIndicesContainer &rootDeviceIndices,
                                      const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields);
    AllocationType getGraphicsAllocationTypeAndCompressionPreference(const UnifiedMemoryProperties &unifiedMemoryProperties, bool &compressionEnabled) const;

    void freeZeroCopySvmAllocation(SvmAllocationData *svmData);

    void initUsmDeviceAllocationsCache(Device &device);
    void initUsmHostAllocationsCache();
    void freeSVMData(SvmAllocationData *svmData);
    void insertSVMAlloc(void *ptr, const SvmAllocationData &allocData);
    void makeResidentForAllocationsWithId(uint32_t allocationId, CommandStreamReceiver &csr);

    SortedVectorBasedAllocationTracker svmAllocs;
    MapOperationsTracker svmMapOperations;
    MapBasedAllocationTracker svmDeferFreeAllocs;
    MemoryManager *memoryManager;
    std::shared_mutex mtx;
    std::mutex mtxForIndirectAccess;
    std::unique_ptr<SvmAllocationCache> usmDeviceAllocationsCache;
    std::unique_ptr<SvmAllocationCache> usmHostAllocationsCache;
    std::multimap<uint32_t, GraphicsAllocation *> internalAllocationsMap;
};
} // namespace NEO

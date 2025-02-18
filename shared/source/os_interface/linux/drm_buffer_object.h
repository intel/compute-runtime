/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/definitions/engine_limits.h"
#include "shared/source/memory_manager/memory_operations_status.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/utilities/stackvec.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <stdint.h>
#include <utility>
#include <vector>

namespace NEO {

struct ExecBuffer;
struct ExecObject;
class DrmMemoryManager;
class Drm;
class OsContext;

class BufferObjectHandleWrapper : NEO::NonCopyableClass {
  private:
    struct ControlBlock {
        int refCount{0};
        int weakRefCount{0};
        std::mutex blockMutex{};
    };

    enum class Ownership : std::uint8_t {
        weak = 0,
        strong = 1,
    };

  public:
    explicit BufferObjectHandleWrapper(int boHandle, uint32_t rootDeviceIndex) noexcept
        : boHandle{boHandle}, rootDeviceIndex(rootDeviceIndex) {}

    BufferObjectHandleWrapper(BufferObjectHandleWrapper &&other) noexcept
        : boHandle(std::exchange(other.boHandle, -1)), rootDeviceIndex(std::exchange(other.rootDeviceIndex, UINT32_MAX)), ownership(other.ownership), controlBlock(std::exchange(other.controlBlock, nullptr)) {}

    ~BufferObjectHandleWrapper();

    BufferObjectHandleWrapper &operator=(BufferObjectHandleWrapper &&) = delete;

    BufferObjectHandleWrapper acquireSharedOwnership();
    BufferObjectHandleWrapper acquireWeakOwnership();

    bool canCloseBoHandle();

    int getBoHandle() const {
        return boHandle;
    }
    uint32_t getRootDeviceIndex() const {
        return rootDeviceIndex;
    }

    void setBoHandle(int handle) {
        boHandle = handle;
    }
    void setRootDeviceIndex(uint32_t index) {
        rootDeviceIndex = index;
    }

  protected:
    BufferObjectHandleWrapper(int boHandle, uint32_t rootDeviceIndex, Ownership ownership, ControlBlock *controlBlock)
        : boHandle{boHandle}, rootDeviceIndex{rootDeviceIndex}, ownership{ownership}, controlBlock{controlBlock} {}

    int boHandle{};
    uint32_t rootDeviceIndex{UINT32_MAX};
    Ownership ownership{Ownership::strong};
    ControlBlock *controlBlock{nullptr};
};

static_assert(NEO::NonCopyable<BufferObjectHandleWrapper>);

class BufferObject {
  public:
    BufferObject(uint32_t rootDeviceIndex, Drm *drm, uint64_t patIndex, int handle, size_t size, size_t maxOsContextCount);
    BufferObject(uint32_t rootDeviceIndex, Drm *drm, uint64_t patIndex, BufferObjectHandleWrapper &&handle, size_t size, size_t maxOsContextCount);

    MOCKABLE_VIRTUAL ~BufferObject() = default;

    struct Deleter {
        void operator()(BufferObject *bo) {
            bo->close();
            delete bo;
        }
    };

    enum class BOType {
        legacy,
        coherent,
        nonCoherent
    };

    bool setTiling(uint32_t mode, uint32_t stride);

    int pin(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId);
    MOCKABLE_VIRTUAL int validateHostPtr(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId);

    MOCKABLE_VIRTUAL int exec(uint32_t used, size_t startOffset, unsigned int flags, bool requiresCoherency, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId,
                              BufferObject *const residency[], size_t residencyCount, ExecObject *execObjectsStorage, uint64_t completionGpuAddress, TaskCountType completionValue);

    int bind(OsContext *osContext, uint32_t vmHandleId, const bool forcePagingFence);
    int unbind(OsContext *osContext, uint32_t vmHandleId);

    void printExecutionBuffer(ExecBuffer &execbuf, const size_t &residencyCount, ExecObject *execObjectsStorage, BufferObject *const residency[]);

    int wait(int64_t timeoutNs);
    bool close();

    inline void reference() {
        this->refCount++;
    }
    inline uint32_t unreference() {
        return this->refCount.fetch_sub(1);
    }
    uint32_t getRefCount() const;

    bool isBoHandleShared() const {
        return boHandleShared;
    }

    void markAsSharedBoHandle() {
        boHandleShared = true;
    }

    BufferObjectHandleWrapper acquireSharedOwnershipOfBoHandle() {
        markAsSharedBoHandle();
        return handle.acquireSharedOwnership();
    }

    BufferObjectHandleWrapper acquireWeakOwnershipOfBoHandle() {
        markAsSharedBoHandle();
        return handle.acquireWeakOwnership();
    }

    size_t peekSize() const { return size; }
    int peekHandle() const { return handle.getBoHandle(); }
    const Drm *peekDrm() const { return drm; }
    uint64_t peekAddress() const { return gpuAddress; }
    void setAddress(uint64_t address);
    void *peekLockedAddress() const { return lockedAddress; }
    void setLockedAddress(void *cpuAddress) { this->lockedAddress = cpuAddress; }
    void setUnmapSize(uint64_t unmapSize) { this->unmapSize = unmapSize; }
    uint64_t peekUnmapSize() const { return unmapSize; }
    bool peekIsReusableAllocation() const { return this->isReused; }
    void markAsReusableAllocation() { this->isReused = true; }
    void addBindExtHandle(uint32_t handle);
    const StackVec<uint32_t, 2> &getBindExtHandles() const { return bindExtHandles; }
    void markForCapture() {
        allowCapture = true;
    }
    bool isMarkedForCapture() {
        return allowCapture;
    }

    bool isImmediateBindingRequired() {
        return requiresImmediateBinding;
    }
    bool isReadOnlyGpuResource() {
        return readOnlyGpuResource;
    }

    void setAsReadOnly(bool isReadOnly) {
        readOnlyGpuResource = isReadOnly;
    }

    void requireImmediateBinding(bool required) {
        requiresImmediateBinding = required;
    }

    bool isExplicitResidencyRequired() {
        return requiresExplicitResidency;
    }
    void requireExplicitResidency(bool required) {
        requiresExplicitResidency = required;
    }
    uint32_t getRootDeviceIndex() {
        return rootDeviceIndex;
    }
    int getHandle() {
        return handle.getBoHandle();
    }

    void setCacheRegion(CacheRegion regionIndex) { cacheRegion = regionIndex; }
    CacheRegion peekCacheRegion() const { return cacheRegion; }

    void setCachePolicy(CachePolicy memType) { cachePolicy = memType; }
    CachePolicy peekCachePolicy() const { return cachePolicy; }

    void setColourWithBind() {
        this->colourWithBind = true;
    }
    void setColourChunk(size_t size) {
        this->colourChunk = size;
    }
    void addColouringAddress(uint64_t address) {
        this->bindAddresses.push_back(address);
    }
    void reserveAddressVector(size_t size) {
        this->bindAddresses.reserve(size);
    }

    bool getColourWithBind() {
        return this->colourWithBind;
    }
    size_t getColourChunk() {
        return this->colourChunk;
    }
    std::vector<uint64_t> &getColourAddresses() {
        return this->bindAddresses;
    }
    void requireExplicitLockedMemory(bool locked) { requiresLocked = locked; }
    bool isExplicitLockedMemoryRequired() { return requiresLocked; }

    uint64_t peekPatIndex() const { return patIndex; }
    void setPatIndex(uint64_t newPatIndex) { this->patIndex = newPatIndex; }
    BOType peekBOType() const { return boType; }
    void setBOType(BOType newBoType) { this->boType = newBoType; }

    void setUserptr(uint64_t ptr) { this->userptr = ptr; };
    uint64_t getUserptr() const { return this->userptr; };

    void setMmapOffset(uint64_t offset) { this->mmapOffset = offset; };
    uint64_t getMmapOffset() const { return this->mmapOffset; };

    static constexpr int gpuHangDetected{-7171};

    uint32_t getOsContextId(OsContext *osContext);

    const auto &getBindInfo() const { return bindInfo; }

    void setChunked(bool chunked) { this->chunked = chunked; }
    bool isChunked() const { return this->chunked; }

    void setRegisteredBindHandleCookie(uint64_t cookie) {
        registeredBindHandleCookie = cookie;
    }

    uint64_t getRegisteredBindHandleCookie() {
        return registeredBindHandleCookie;
    }

  protected:
    MOCKABLE_VIRTUAL MemoryOperationsStatus evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded);
    MOCKABLE_VIRTUAL void fillExecObject(ExecObject &execObject, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId);
    void printBOBindingResult(OsContext *osContext, uint32_t vmHandleId, bool bind, int retVal);

    Drm *drm = nullptr;
    BufferObjectHandleWrapper handle; // i915 gem object handle
    void *lockedAddress = nullptr;    // CPU side virtual address

    uint64_t size = 0;
    uint64_t unmapSize = 0;
    uint64_t patIndex = CommonConstants::unsupportedPatIndex;
    uint64_t userptr = 0u;
    uint64_t mmapOffset = 0u;
    size_t colourChunk = 0;
    uint64_t gpuAddress = 0llu;

    std::vector<uint64_t> bindAddresses;
    std::vector<std::array<bool, EngineLimits::maxHandleCount>> bindInfo;
    StackVec<uint32_t, 2> bindExtHandles;
    uint64_t registeredBindHandleCookie = 0;
    BOType boType = BOType::legacy;
    std::atomic<uint32_t> refCount;
    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();
    uint32_t tilingMode = 0;
    CachePolicy cachePolicy = CachePolicy::writeBack;
    CacheRegion cacheRegion = CacheRegion::defaultRegion;

    bool colourWithBind = false;
    bool perContextVmsUsed = false;
    bool boHandleShared = false;
    bool allowCapture = false;
    bool requiresImmediateBinding = false;
    bool requiresExplicitResidency = false;
    bool requiresLocked = false;
    bool chunked = false;
    bool isReused = false;
    bool readOnlyGpuResource = false;
};
} // namespace NEO

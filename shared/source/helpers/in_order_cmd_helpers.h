/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_type.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace NEO {
class GraphicsAllocation;
class MemoryManager;
class Device;
class TagNodeBase;

template <bool deviceAlloc>
class DeviceAllocNodeType {
  public:
    using ValueT = uint64_t;

    static constexpr size_t defaultAllocatorTagCount = 128;

    static constexpr AllocationType getAllocationType() { return deviceAlloc ? NEO::AllocationType::gpuTimestampDeviceBuffer : NEO::AllocationType::timestampPacketTagBuffer; }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::counter64b; }

    static constexpr size_t getSinglePacketSize() { return sizeof(uint64_t); }

    void initialize(uint64_t initValue) { data = initValue; }

  protected:
    uint64_t data = {};
};

static_assert(sizeof(uint64_t) == sizeof(DeviceAllocNodeType<true>), "This structure is consumed by GPU and has to follow specific restrictions for padding and size");
static_assert(sizeof(uint64_t) == sizeof(DeviceAllocNodeType<false>), "This structure is consumed by GPU and has to follow specific restrictions for padding and size");

class InOrderExecInfo : public NEO::NonCopyableClass {
  public:
    ~InOrderExecInfo();

    InOrderExecInfo() = delete;

    static std::shared_ptr<InOrderExecInfo> create(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::Device &device, uint32_t partitionCount, bool regularCmdList);
    static std::shared_ptr<InOrderExecInfo> createFromExternalAllocation(NEO::Device &device, NEO::GraphicsAllocation *deviceAllocation, uint64_t deviceAddress, NEO::GraphicsAllocation *hostAllocation,
                                                                         uint64_t *hostAddress, uint64_t counterValue, uint32_t devicePartitions, uint32_t hostPartitions);

    InOrderExecInfo(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::Device &device, uint32_t partitionCount, bool regularCmdList, bool atomicDeviceSignalling);

    NEO::GraphicsAllocation *getDeviceCounterAllocation() const;
    NEO::GraphicsAllocation *getHostCounterAllocation() const;
    uint64_t *getBaseHostAddress() const { return hostAddress; }
    uint64_t getBaseDeviceAddress() const { return deviceAddress; }
    uint64_t getBaseHostGpuAddress() const;

    uint64_t getCounterValue() const { return counterValue; }
    void addCounterValue(uint64_t addValue) { counterValue += addValue; }
    void resetCounterValue() { counterValue = 0; }

    uint64_t getRegularCmdListSubmissionCounter() const { return regularCmdListSubmissionCounter; }
    void addRegularCmdListSubmissionCounter(uint64_t addValue) { regularCmdListSubmissionCounter += addValue; }

    bool isRegularCmdList() const { return regularCmdList; }
    bool isHostStorageDuplicated() const { return duplicatedHostStorage; }
    bool isAtomicDeviceSignalling() const { return atomicDeviceSignalling; }

    uint32_t getNumDevicePartitionsToWait() const { return numDevicePartitionsToWait; }
    uint32_t getNumHostPartitionsToWait() const { return numHostPartitionsToWait; }

    void setAllocationOffset(uint32_t newOffset) { allocationOffset = newOffset; }
    void initializeAllocationsFromHost();
    uint32_t getAllocationOffset() const { return allocationOffset; }

    void reset();
    bool isExternalMemoryExecInfo() const { return deviceCounterNode == nullptr; }
    void setLastWaitedCounterValue(uint64_t value) {
        if (!isExternalMemoryExecInfo()) {
            lastWaitedCounterValue = std::max(value, lastWaitedCounterValue);
        }
    }

    bool isCounterAlreadyDone(uint64_t waitValue) const {
        return lastWaitedCounterValue >= waitValue && this->allocationOffset == 0u;
    }

    NEO::GraphicsAllocation *getExternalHostAllocation() const { return externalHostAllocation; }
    NEO::GraphicsAllocation *getExternalDeviceAllocation() const { return externalDeviceAllocation; }

    void pushTempTimestampNode(TagNodeBase *node, uint64_t value);
    void releaseNotUsedTempTimestampNodes(bool forceReturn);

  protected:
    void uploadToTbx(TagNodeBase &node, size_t size);

    NEO::Device &device;
    NEO::TagNodeBase *deviceCounterNode = nullptr;
    NEO::TagNodeBase *hostCounterNode = nullptr;
    NEO::GraphicsAllocation *externalHostAllocation = nullptr;
    NEO::GraphicsAllocation *externalDeviceAllocation = nullptr;
    std::vector<std::pair<NEO::TagNodeBase *, uint64_t>> tempTimestampNodes;

    std::mutex mutex;

    uint64_t counterValue = 0;
    uint64_t lastWaitedCounterValue = 0;
    uint64_t regularCmdListSubmissionCounter = 0;
    uint64_t deviceAddress = 0;
    uint64_t *hostAddress = nullptr;
    uint32_t numDevicePartitionsToWait = 0;
    uint32_t numHostPartitionsToWait = 0;
    uint32_t allocationOffset = 0;
    uint32_t rootDeviceIndex = 0;
    bool regularCmdList = false;
    bool duplicatedHostStorage = false;
    bool atomicDeviceSignalling = false;
    bool isTbx = false;
};

namespace InOrderPatchCommandHelpers {
inline uint64_t getAppendCounterValue(const InOrderExecInfo &inOrderExecInfo) {
    if (inOrderExecInfo.isRegularCmdList() && inOrderExecInfo.getRegularCmdListSubmissionCounter() > 1) {
        return inOrderExecInfo.getCounterValue() * (inOrderExecInfo.getRegularCmdListSubmissionCounter() - 1);
    }

    return 0;
}

enum class PatchCmdType {
    none,
    lri64b,
    sdi,
    semaphore,
    walker,
    pipeControl,
    xyCopyBlt,
    xyBlockCopyBlt,
    xyColorBlt,
    memSet
};

template <typename GfxFamily>
struct PatchCmd {
    PatchCmd(std::shared_ptr<InOrderExecInfo> *inOrderExecInfo, void *cmd1, void *cmd2, uint64_t baseCounterValue, PatchCmdType patchCmdType, bool deviceAtomicSignaling, bool duplicatedHostStorage)
        : cmd1(cmd1), cmd2(cmd2), baseCounterValue(baseCounterValue), patchCmdType(patchCmdType), deviceAtomicSignaling(deviceAtomicSignaling), duplicatedHostStorage(duplicatedHostStorage) {
        if (inOrderExecInfo) {
            this->inOrderExecInfo = *inOrderExecInfo;
        }
    }

    void patch(uint64_t appendCounterValue) {
        if (skipPatching) {
            return;
        }
        switch (patchCmdType) {
        case PatchCmdType::sdi:
            patchSdi(appendCounterValue);
            break;
        case PatchCmdType::semaphore:
            patchSemaphore(appendCounterValue);
            break;
        case PatchCmdType::walker:
            patchComputeWalker(appendCounterValue);
            break;
        case PatchCmdType::lri64b:
            patchLri64b(appendCounterValue);
            break;
        case PatchCmdType::pipeControl:
            patchPipeControl(appendCounterValue);
            break;
        case PatchCmdType::xyCopyBlt:
        case PatchCmdType::xyBlockCopyBlt:
        case PatchCmdType::xyColorBlt:
        case PatchCmdType::memSet:
            patchBlitterCommand(appendCounterValue, patchCmdType);
            break;
        default:
            UNRECOVERABLE_IF(true);
            break;
        }
    }

    void updateInOrderExecInfo(std::shared_ptr<InOrderExecInfo> *inOrderExecInfo) {
        this->inOrderExecInfo = *inOrderExecInfo;
    }

    void setSkipPatching(bool value) {
        skipPatching = value;
    }

    bool isExternalDependency() const { return inOrderExecInfo.get(); }

    std::shared_ptr<InOrderExecInfo> inOrderExecInfo;
    void *cmd1 = nullptr;
    void *cmd2 = nullptr;
    const uint64_t baseCounterValue = 0;
    const PatchCmdType patchCmdType = PatchCmdType::none;
    bool deviceAtomicSignaling = false;
    bool duplicatedHostStorage = false;
    bool skipPatching = false;

  protected:
    void patchSdi(uint64_t appendCounterValue) {
        auto sdiCmd = reinterpret_cast<typename GfxFamily::MI_STORE_DATA_IMM *>(cmd1);
        sdiCmd->setDataDword0(getLowPart(baseCounterValue + appendCounterValue));
        sdiCmd->setDataDword1(getHighPart(baseCounterValue + appendCounterValue));
    }

    void patchSemaphore(uint64_t appendCounterValue) {
        if (isExternalDependency()) {
            appendCounterValue = InOrderPatchCommandHelpers::getAppendCounterValue(*inOrderExecInfo);
            if (appendCounterValue == 0) {
                return;
            }
        }

        auto semaphoreCmd = reinterpret_cast<typename GfxFamily::MI_SEMAPHORE_WAIT *>(cmd1);
        semaphoreCmd->setSemaphoreDataDword(static_cast<uint32_t>(baseCounterValue + appendCounterValue));
    }

    void patchComputeWalker(uint64_t appendCounterValue);
    void patchBlitterCommand(uint64_t appendCounterValue, PatchCmdType patchCmdType);

    void patchPipeControl(uint64_t appendCounterValue) {
        auto pcCmd = reinterpret_cast<typename GfxFamily::PIPE_CONTROL *>(cmd1);
        pcCmd->setImmediateData(static_cast<uint64_t>(baseCounterValue + appendCounterValue));
    }

    void patchLri64b(uint64_t appendCounterValue) {
        if (isExternalDependency()) {
            appendCounterValue = InOrderPatchCommandHelpers::getAppendCounterValue(*inOrderExecInfo);
            if (appendCounterValue == 0) {
                return;
            }
        }

        const uint64_t counterValue = baseCounterValue + appendCounterValue;

        auto lri1 = reinterpret_cast<typename GfxFamily::MI_LOAD_REGISTER_IMM *>(cmd1);
        lri1->setDataDword(getLowPart(counterValue));

        auto lri2 = reinterpret_cast<typename GfxFamily::MI_LOAD_REGISTER_IMM *>(cmd2);
        lri2->setDataDword(getHighPart(counterValue));
    }

    PatchCmd() = delete;
};

} // namespace InOrderPatchCommandHelpers

template <typename GfxFamily>
using InOrderPatchCommandsContainer = std::vector<NEO::InOrderPatchCommandHelpers::PatchCmd<GfxFamily>>;

} // namespace NEO

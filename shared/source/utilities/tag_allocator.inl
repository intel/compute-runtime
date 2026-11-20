/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/timestamp_packet_constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/sys_calls_common.h"

#include <algorithm>

namespace NEO {
template <typename TagType>
void TagNode<TagType>::initialize() {
    auto *tagAllocator = static_cast<TagAllocator<TagType> *>(allocator);
    UNRECOVERABLE_IF(tagAllocator == nullptr);

    tagForCpuAccess->initialize(tagAllocator->getInitialValue());
    packetsUsed = 1;
    setProfilingCapable(true);
    tagAllocator->notifyTagNodeUpdated(*this, 0, tagAllocator->getTagSize());
}

template <typename TagType>
void TagNode<TagType>::markAsAborted() {
    tagForCpuAccess->initialize(0);
    if (allocator != nullptr) {
        allocator->notifyTagNodeUpdated(*this, 0, allocator->getTagSize());
    }
}

template <typename TagType>
TagAllocator<TagType>::TagAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memMngr, uint32_t tagCount, size_t tagAlignment,
                                    size_t tagSize, ValueT initialValue, bool doNotReleaseNodes, bool initializeTags, DeviceBitfield deviceBitfield)
    : TagAllocatorBase(rootDeviceIndices, memMngr, tagCount, tagAlignment, tagSize, doNotReleaseNodes, deviceBitfield), initialValue(initialValue), initializeTags(initializeTags) {

    populateFreeTags();
}

template <typename TagType>
TagNodeBase *TagAllocator<TagType>::getTag() {
    auto node = this->popFreeTag();
    if (node == nullptr) {
        this->releaseDeferredTags();
        node = this->popFreeTag();
    }
    if (node == nullptr) {
        std::unique_lock<std::mutex> lock(this->allocatorMutex);
        node = this->createTagPool();
        if (node == nullptr) {
            return nullptr;
        }
        this->pushToFreeTagsInReverseOrder(node + 1, this->tagCount - 1);
    }
    node->incRefCount();

    if (initializeTags) {
        node->initialize();
    }

    PRINT_STRING(debugManager.flags.PrintTimestampPacketUsage.get() == 1, stdout,
                 "\nPID: %u, TSP taken from pool and initialized(%d): 0x%" PRIX64, SysCalls::getProcessId(), initializeTags, node->getGpuAddress());

    return node;
}

template <typename TagType>
typename TagAllocator<TagType>::NodeType *TagAllocator<TagType>::popFreeTag() {
    std::lock_guard<SpinLock> lock(this->tagListsLock);
    if (this->freeTags.empty()) {
        return nullptr;
    }
    auto node = this->freeTags.back();
    this->freeTags.pop_back();
    return node;
}

template <typename TagType>
void TagAllocator<TagType>::returnTagToFreePool(TagNodeBase *node) {
    auto nodeT = static_cast<NodeType *>(node);

    PRINT_STRING(debugManager.flags.PrintTimestampPacketUsage.get() == 1, stdout,
                 "\nPID: %u, TSP returned to pool: 0x%" PRIX64, SysCalls::getProcessId(), nodeT->getGpuAddress());

    std::lock_guard<SpinLock> lock(this->tagListsLock);
    this->freeTags.push_back(nodeT);
}

template <typename TagType>
void TagAllocator<TagType>::returnTagToDeferredPool(TagNodeBase *node) {
    auto nodeT = static_cast<NodeType *>(node);
    std::lock_guard<SpinLock> lock(this->tagListsLock);
    this->deferredTags.push_back(nodeT);
}

template <typename TagType>
void TagAllocator<TagType>::releaseDeferredTags() {
    std::vector<NodeType *> pendingTags;
    {
        std::lock_guard<SpinLock> lock(this->tagListsLock);
        if (this->deferredTags.empty()) {
            return;
        }
        pendingTags.swap(this->deferredTags);
    }

    const auto firstNotReleasedTag = std::partition(pendingTags.begin(), pendingTags.end(), [](const NodeType *node) {
        if (!node->canBeReleased()) {
            return false;
        }
        PRINT_STRING(debugManager.flags.PrintTimestampPacketUsage.get() == 1, stdout,
                     "\nPID: %u, TSP returned to pool: 0x%" PRIX64, SysCalls::getProcessId(), node->getGpuAddress());
        return true;
    });

    std::lock_guard<SpinLock> lock(this->tagListsLock);
    this->freeTags.insert(this->freeTags.end(), pendingTags.begin(), firstNotReleasedTag);
    this->deferredTags.insert(this->deferredTags.end(), firstNotReleasedTag, pendingTags.end());
}

template <typename TagType>
bool TagAllocator<TagType>::populateFreeTags() {
    auto nodes = this->createTagPool();
    if (nodes == nullptr) {
        return false;
    }
    this->pushToFreeTagsInReverseOrder(nodes, this->tagCount);
    return true;
}

template <typename TagType>
void TagAllocator<TagType>::pushToFreeTagsInReverseOrder(NodeType *nodes, uint32_t nodesCount) {
    std::lock_guard<SpinLock> lock(this->tagListsLock);
    this->freeTags.reserve(this->freeTags.size() + nodesCount);
    for (auto nodeIndex = nodesCount; nodeIndex > 0; --nodeIndex) {
        this->freeTags.push_back(&nodes[nodeIndex - 1]);
    }
}

template <typename TagType>
typename TagAllocator<TagType>::NodeType *TagAllocator<TagType>::createTagPool() {
    size_t allocationSizeRequired = tagCount * tagSize;

    void *baseCpuAddress = nullptr;
    uint64_t baseGpuAddress = 0;

    auto multiGraphicsAllocation = new MultiGraphicsAllocation(maxRootDeviceIndex);
    AllocationProperties allocationProperties{rootDeviceIndices[0], allocationSizeRequired, TagType::getAllocationType(), deviceBitfield};

    if (rootDeviceIndices.size() == 1) {
        GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
        if (graphicsAllocation == nullptr) {
            delete multiGraphicsAllocation;
            return nullptr;
        }

        baseCpuAddress = graphicsAllocation->getUnderlyingBuffer();
        baseGpuAddress = graphicsAllocation->getGpuAddress();

        multiGraphicsAllocation->addAllocation(graphicsAllocation);
    } else {
        allocationProperties.subDevicesBitfield = systemMemoryBitfield;

        baseCpuAddress = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, allocationProperties, *multiGraphicsAllocation);
        if (baseCpuAddress == nullptr) {
            delete multiGraphicsAllocation;
            return nullptr;
        }
        baseGpuAddress = castToUint64(baseCpuAddress);
    }

    gfxAllocations.emplace_back(multiGraphicsAllocation);

    notifyTagPoolCreated(*multiGraphicsAllocation);

    auto nodesMemory = std::make_unique_for_overwrite<NodeType[]>(tagCount);

    for (size_t i = 0; i < tagCount; ++i) {
        auto tagOffset = i * tagSize;

        nodesMemory[i].allocator = this;
        nodesMemory[i].gfxAllocation = multiGraphicsAllocation;
        nodesMemory[i].tagForCpuAccess = reinterpret_cast<TagType *>(ptrOffset(baseCpuAddress, tagOffset));
        nodesMemory[i].gpuAddress = baseGpuAddress + tagOffset;
        nodesMemory[i].setDoNotReleaseNodes(doNotReleaseNodes);
    }

    auto nodes = nodesMemory.get();
    tagPoolMemory.push_back(std::move(nodesMemory));
    return nodes;
}

template <typename TagType>
void TagAllocator<TagType>::returnTag(TagNodeBase *node) {
    if (node->refCountFetchSub(1) == 1) {
        if (node->canBeReleased()) {
            returnTagToFreePool(node);
        } else {
            returnTagToDeferredPool(node);
        }
    }
}

template <typename TagType>
size_t TagNode<TagType>::getGlobalStartOffset() const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket) {
        return TagType::getGlobalStartOffset();
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
size_t TagNode<TagType>::getContextStartOffset() const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket) {
        return TagType::getContextStartOffset();
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
size_t TagNode<TagType>::getContextEndOffset() const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket) {
        return TagType::getContextEndOffset();
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
size_t TagNode<TagType>::getGlobalEndOffset() const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket) {
        return TagType::getGlobalEndOffset();
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
uint64_t TagNode<TagType>::getContextStartValue([[maybe_unused]] uint32_t packetIndex) const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket || TagType::getTagNodeType() == TagNodeType::hwTimeStamps) {
        return tagForCpuAccess->getContextStartValue(packetIndex);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
uint64_t TagNode<TagType>::getGlobalStartValue([[maybe_unused]] uint32_t packetIndex) const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket || TagType::getTagNodeType() == TagNodeType::hwTimeStamps) {
        return tagForCpuAccess->getGlobalStartValue(packetIndex);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
uint64_t TagNode<TagType>::getContextEndValue([[maybe_unused]] uint32_t packetIndex) const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket || TagType::getTagNodeType() == TagNodeType::hwTimeStamps) {
        return tagForCpuAccess->getContextEndValue(packetIndex);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
uint64_t TagNode<TagType>::getGlobalEndValue([[maybe_unused]] uint32_t packetIndex) const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket || TagType::getTagNodeType() == TagNodeType::hwTimeStamps) {
        return tagForCpuAccess->getGlobalEndValue(packetIndex);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
void TagNode<TagType>::getGlobalTimestampValues([[maybe_unused]] uint32_t packetIndex, [[maybe_unused]] uint64_t &globalStart, [[maybe_unused]] uint64_t &globalEnd) const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket || TagType::getTagNodeType() == TagNodeType::hwTimeStamps) {
        tagForCpuAccess->getGlobalTimestampValues(packetIndex, globalStart, globalEnd);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
void const *TagNode<TagType>::getContextEndAddress([[maybe_unused]] uint32_t packetIndex) const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket) {
        return tagForCpuAccess->getContextEndAddress(packetIndex);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
uint64_t &TagNode<TagType>::getContextCompleteRef() const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::hwTimeStamps) {
        return tagForCpuAccess->contextCompleteTS;
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
uint64_t &TagNode<TagType>::getGlobalEndRef() const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::hwTimeStamps) {
        return tagForCpuAccess->globalEndTS;
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
size_t TagNode<TagType>::getSinglePacketSize() const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket) {
        return TagType::getSinglePacketSize();
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
void TagNode<TagType>::assignDataToAllTimestamps([[maybe_unused]] uint32_t packetIndex, [[maybe_unused]] const void *source) {
    if constexpr (TagType::getTagNodeType() == TagNodeType::timestampPacket) {
        UNRECOVERABLE_IF(packetIndex >= tagForCpuAccess->getPacketCount());
        constexpr size_t packetSize = TagType::getSinglePacketSize();
        tagForCpuAccess->assignDataToAllTimestamps(packetIndex, source);
        if (allocator != nullptr) {
            const uint64_t packetOffset = static_cast<uint64_t>(packetIndex) * packetSize;
            allocator->notifyTagNodeUpdated(*this, packetOffset, packetSize);
        }
        return;
    } else {
        UNRECOVERABLE_IF(true);
    }
}

template <typename TagType>
MetricsLibraryApi::QueryHandle_1_0 &TagNode<TagType>::getQueryHandleRef() const {
    if constexpr (TagType::getTagNodeType() == TagNodeType::hwPerfCounter) {
        return tagForCpuAccess->query.handle;
    } else {
        UNRECOVERABLE_IF(true);
    }
}

} // namespace NEO

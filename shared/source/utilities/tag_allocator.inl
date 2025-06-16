/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/timestamp_packet_constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/sys_calls_common.h"

namespace NEO {
template <typename TagType>
TagAllocator<TagType>::TagAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memMngr, size_t tagCount, size_t tagAlignment,
                                    size_t tagSize, ValueT initialValue, bool doNotReleaseNodes, bool initializeTags, DeviceBitfield deviceBitfield)
    : TagAllocatorBase(rootDeviceIndices, memMngr, tagCount, tagAlignment, tagSize, doNotReleaseNodes, deviceBitfield), initialValue(initialValue), initializeTags(initializeTags) {

    populateFreeTags();
}

template <typename TagType>
TagNodeBase *TagAllocator<TagType>::getTag() {
    if (freeTags.peekIsEmpty()) {
        releaseDeferredTags();
    }
    auto node = freeTags.removeFrontOne().release();
    if (!node) {
        std::unique_lock<std::mutex> lock(allocatorMutex);
        populateFreeTags();
        node = freeTags.removeFrontOne().release();
    }
    usedTags.pushFrontOne(*node);
    node->incRefCount();

    if (initializeTags) {
        node->initialize();
    }

    if (debugManager.flags.PrintTimestampPacketUsage.get() == 1) {
        printf("\nPID: %u, TSP taken from pool and initialized(%d): 0x%" PRIX64, SysCalls::getProcessId(), initializeTags, node->getGpuAddress());
    }

    return node;
}

template <typename TagType>
void TagAllocator<TagType>::returnTagToFreePool(TagNodeBase *node) {
    auto nodeT = static_cast<NodeType *>(node);
    [[maybe_unused]] auto usedNode = usedTags.removeOne(*nodeT).release();
    DEBUG_BREAK_IF(usedNode == nullptr);

    if (debugManager.flags.PrintTimestampPacketUsage.get() == 1) {
        printf("\nPID: %u, TSP returned to pool: 0x%" PRIX64, SysCalls::getProcessId(), nodeT->getGpuAddress());
    }

    freeTags.pushFrontOne(*nodeT);
}

template <typename TagType>
void TagAllocator<TagType>::returnTagToDeferredPool(TagNodeBase *node) {
    auto nodeT = static_cast<NodeType *>(node);
    auto usedNode = usedTags.removeOne(*nodeT).release();
    DEBUG_BREAK_IF(!usedNode);
    deferredTags.pushFrontOne(*usedNode);
}

template <typename TagType>
void TagAllocator<TagType>::releaseDeferredTags() {
    IDList<NodeType, false> pendingFreeTags;
    IDList<NodeType, false> pendingDeferredTags;
    auto currentNode = deferredTags.detachNodes();

    while (currentNode != nullptr) {
        auto nextNode = currentNode->next;
        if (currentNode->canBeReleased()) {
            if (debugManager.flags.PrintTimestampPacketUsage.get() == 1) {
                printf("\nPID: %u, TSP returned to pool: 0x%" PRIX64, SysCalls::getProcessId(), currentNode->getGpuAddress());
            }
            pendingFreeTags.pushFrontOne(*currentNode);
        } else {
            pendingDeferredTags.pushFrontOne(*currentNode);
        }
        currentNode = nextNode;
    }

    if (!pendingFreeTags.peekIsEmpty()) {
        freeTags.splice(*pendingFreeTags.detachNodes());
    }
    if (!pendingDeferredTags.peekIsEmpty()) {
        deferredTags.splice(*pendingDeferredTags.detachNodes());
    }
}

template <typename TagType>
void TagAllocator<TagType>::populateFreeTags() {
    size_t allocationSizeRequired = tagCount * tagSize;

    void *baseCpuAddress = nullptr;
    uint64_t baseGpuAddress = 0;

    auto multiGraphicsAllocation = new MultiGraphicsAllocation(maxRootDeviceIndex);
    AllocationProperties allocationProperties{rootDeviceIndices[0], allocationSizeRequired, TagType::getAllocationType(), deviceBitfield};

    if (rootDeviceIndices.size() == 1) {
        GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);

        baseCpuAddress = graphicsAllocation->getUnderlyingBuffer();
        baseGpuAddress = graphicsAllocation->getGpuAddress();

        multiGraphicsAllocation->addAllocation(graphicsAllocation);
    } else {
        allocationProperties.subDevicesBitfield = systemMemoryBitfield;

        baseCpuAddress = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, allocationProperties, *multiGraphicsAllocation);
        baseGpuAddress = castToUint64(baseCpuAddress);
    }

    gfxAllocations.emplace_back(multiGraphicsAllocation);

    auto nodesMemory = std::make_unique_for_overwrite<NodeType[]>(tagCount);

    for (size_t i = 0; i < tagCount; ++i) {
        auto tagOffset = i * tagSize;

        nodesMemory[i].allocator = this;
        nodesMemory[i].gfxAllocation = multiGraphicsAllocation;
        nodesMemory[i].tagForCpuAccess = reinterpret_cast<TagType *>(ptrOffset(baseCpuAddress, tagOffset));
        nodesMemory[i].gpuAddress = baseGpuAddress + tagOffset;
        nodesMemory[i].setDoNotReleaseNodes(doNotReleaseNodes);

        freeTags.pushTailOne(nodesMemory[i]);
    }

    tagPoolMemory.push_back(std::move(nodesMemory));
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
        return tagForCpuAccess->assignDataToAllTimestamps(packetIndex, source);
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

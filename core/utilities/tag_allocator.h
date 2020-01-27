/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/aligned_memory.h"
#include "core/helpers/debug_helpers.h"
#include "core/utilities/idlist.h"
#include "runtime/memory_manager/memory_manager.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace NEO {
class GraphicsAllocation;

template <typename TagType>
class TagAllocator;

template <typename TagType>
struct TagNode : public IDNode<TagNode<TagType>> {
  public:
    TagType *tagForCpuAccess;

    GraphicsAllocation *getBaseGraphicsAllocation() const { return gfxAllocation; }
    uint64_t getGpuAddress() const { return gpuAddress; }

    void incRefCount() { refCount++; }

    MOCKABLE_VIRTUAL void returnTag() {
        allocator->returnTag(this);
    }

    bool canBeReleased() const {
        return !doNotReleaseNodes && tagForCpuAccess->isCompleted();
    }

    void setDoNotReleaseNodes(bool doNotRelease) {
        doNotReleaseNodes = doNotRelease;
    }

  protected:
    TagAllocator<TagType> *allocator = nullptr;
    GraphicsAllocation *gfxAllocation = nullptr;
    uint64_t gpuAddress = 0;
    std::atomic<uint32_t> refCount{0};
    bool doNotReleaseNodes = false;

    template <typename TagType2>
    friend class TagAllocator;
};

template <typename TagType>
class TagAllocator {
  public:
    using NodeType = TagNode<TagType>;

    TagAllocator(uint32_t rootDeviceIndex, MemoryManager *memMngr, size_t tagCount,
                 size_t tagAlignment, size_t tagSize, bool doNotReleaseNodes) : rootDeviceIndex(rootDeviceIndex),
                                                                                memoryManager(memMngr),
                                                                                tagCount(tagCount),
                                                                                tagAlignment(tagAlignment),
                                                                                doNotReleaseNodes(doNotReleaseNodes) {

        this->tagSize = alignUp(tagSize, tagAlignment);
        populateFreeTags();
    }

    MOCKABLE_VIRTUAL ~TagAllocator() {
        cleanUpResources();
    }

    void cleanUpResources() {
        for (auto gfxAllocation : gfxAllocations) {
            memoryManager->freeGraphicsMemory(gfxAllocation);
        }
        gfxAllocations.clear();

        for (auto nodesMemory : tagPoolMemory) {
            delete[] nodesMemory;
        }
        tagPoolMemory.clear();
    }

    NodeType *getTag() {
        if (freeTags.peekIsEmpty()) {
            releaseDeferredTags();
        }
        NodeType *node = freeTags.removeFrontOne().release();
        if (!node) {
            std::unique_lock<std::mutex> lock(allocatorMutex);
            populateFreeTags();
            node = freeTags.removeFrontOne().release();
        }
        usedTags.pushFrontOne(*node);
        node->incRefCount();
        node->tagForCpuAccess->initialize();
        return node;
    }

    MOCKABLE_VIRTUAL void returnTag(NodeType *node) {
        if (node->refCount.fetch_sub(1) == 1) {
            if (node->canBeReleased()) {
                returnTagToFreePool(node);
            } else {
                returnTagToDeferredPool(node);
            }
        }
    }

  protected:
    IDList<NodeType> freeTags;
    IDList<NodeType> usedTags;
    IDList<NodeType> deferredTags;
    std::vector<GraphicsAllocation *> gfxAllocations;
    std::vector<NodeType *> tagPoolMemory;

    const uint32_t rootDeviceIndex;
    MemoryManager *memoryManager;
    size_t tagCount;
    size_t tagAlignment;
    size_t tagSize;
    bool doNotReleaseNodes = false;

    std::mutex allocatorMutex;

    MOCKABLE_VIRTUAL void returnTagToFreePool(NodeType *node) {
        NodeType *usedNode = usedTags.removeOne(*node).release();
        DEBUG_BREAK_IF(usedNode == nullptr);
        UNUSED_VARIABLE(usedNode);
        freeTags.pushFrontOne(*node);
    }

    void returnTagToDeferredPool(NodeType *node) {
        NodeType *usedNode = usedTags.removeOne(*node).release();
        DEBUG_BREAK_IF(!usedNode);
        deferredTags.pushFrontOne(*usedNode);
    }

    void populateFreeTags() {
        size_t allocationSizeRequired = tagCount * tagSize;

        auto allocationType = TagType::getAllocationType();
        GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, allocationSizeRequired, allocationType});
        gfxAllocations.push_back(graphicsAllocation);

        uint64_t gpuBaseAddress = graphicsAllocation->getGpuAddress();
        uintptr_t Size = graphicsAllocation->getUnderlyingBufferSize();
        uintptr_t Start = reinterpret_cast<uintptr_t>(graphicsAllocation->getUnderlyingBuffer());
        uintptr_t End = Start + Size;

        NodeType *nodesMemory = new NodeType[tagCount];

        for (size_t i = 0; i < tagCount; ++i) {
            nodesMemory[i].allocator = this;
            nodesMemory[i].gfxAllocation = graphicsAllocation;
            nodesMemory[i].tagForCpuAccess = reinterpret_cast<TagType *>(Start);
            nodesMemory[i].gpuAddress = gpuBaseAddress + (i * tagSize);
            nodesMemory[i].setDoNotReleaseNodes(doNotReleaseNodes);
            freeTags.pushTailOne(nodesMemory[i]);
            Start += tagSize;
        }
        DEBUG_BREAK_IF(Start > End);
        UNUSED_VARIABLE(End);
        tagPoolMemory.push_back(nodesMemory);
    }

    void releaseDeferredTags() {
        IDList<NodeType, false> pendingFreeTags;
        IDList<NodeType, false> pendingDeferredTags;
        auto currentNode = deferredTags.detachNodes();

        while (currentNode != nullptr) {
            auto nextNode = currentNode->next;
            if (currentNode->canBeReleased()) {
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
};
} // namespace NEO

/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/utilities/idlist.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace OCLRT {
class GraphicsAllocation;

template <typename TagType>
class TagAllocator;

template <typename TagType>
struct TagNode : public IDNode<TagNode<TagType>> {
  public:
    TagType *tag;

    GraphicsAllocation *getGraphicsAllocation() const { return gfxAllocation; }

    void incRefCount() { refCount++; }

    void returnTag() {
        allocator->returnTag(this);
    }

  protected:
    TagNode() = default;
    TagAllocator<TagType> *allocator;
    GraphicsAllocation *gfxAllocation;
    std::atomic<uint32_t> refCount{0};

    template <typename TagType2>
    friend class TagAllocator;
};

template <typename TagType>
class TagAllocator {
  public:
    using NodeType = TagNode<TagType>;

    TagAllocator(MemoryManager *memMngr, size_t tagCount, size_t tagAlignment) : memoryManager(memMngr),
                                                                                 tagCount(tagCount),
                                                                                 tagAlignment(tagAlignment) {
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
        node->tag->initialize();
        return node;
    }

    MOCKABLE_VIRTUAL void returnTag(NodeType *node) {
        if (node->refCount.fetch_sub(1) == 1) {
            if (node->tag->canBeReleased()) {
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

    MemoryManager *memoryManager;
    size_t tagCount;
    size_t tagAlignment;

    std::mutex allocatorMutex;

    MOCKABLE_VIRTUAL void returnTagToFreePool(NodeType *node) {
        NodeType *usedNode = usedTags.removeOne(*node).release();
        DEBUG_BREAK_IF(usedNode == nullptr);
        ((void)(usedNode));
        freeTags.pushFrontOne(*node);
    }

    void returnTagToDeferredPool(NodeType *node) {
        NodeType *usedNode = usedTags.removeOne(*node).release();
        DEBUG_BREAK_IF(!usedNode);
        deferredTags.pushFrontOne(*usedNode);
    }

    void populateFreeTags() {
        size_t tagSize = alignUp(sizeof(TagType), tagAlignment);
        size_t allocationSizeRequired = tagCount * tagSize;

        GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties({allocationSizeRequired, GraphicsAllocation::AllocationType::TIMESTAMP_TAG_BUFFER});
        gfxAllocations.push_back(graphicsAllocation);

        uintptr_t Size = graphicsAllocation->getUnderlyingBufferSize();
        uintptr_t Start = reinterpret_cast<uintptr_t>(graphicsAllocation->getUnderlyingBuffer());
        uintptr_t End = Start + Size;
        size_t nodeCount = Size / tagSize;

        NodeType *nodesMemory = new NodeType[nodeCount];

        for (size_t i = 0; i < nodeCount; ++i) {
            nodesMemory[i].allocator = this;
            nodesMemory[i].gfxAllocation = graphicsAllocation;
            nodesMemory[i].tag = reinterpret_cast<TagType *>(Start);
            freeTags.pushTailOne(nodesMemory[i]);
            Start += tagSize;
        }
        DEBUG_BREAK_IF(Start > End);
        ((void)(End));
        tagPoolMemory.push_back(nodesMemory);
    }

    void releaseDeferredTags() {
        IDList<NodeType, false> pendingFreeTags;
        IDList<NodeType, false> pendingDeferredTags;
        auto currentNode = deferredTags.detachNodes();

        while (currentNode != nullptr) {
            auto nextNode = currentNode->next;
            if (currentNode->tag->canBeReleased()) {
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
} // namespace OCLRT

/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
struct TagNode : public IDNode<TagNode<TagType>> {
  public:
    TagType *tag;
    GraphicsAllocation *getGraphicsAllocation() {
        return gfxAllocation;
    }

    void incRefCount() { refCount++; }

  protected:
    TagNode() = default;
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
        size_t size = gfxAllocations.size();

        for (uint32_t i = 0; i < size; ++i) {
            memoryManager->freeGraphicsMemory(gfxAllocations[i]);
        }
        gfxAllocations.clear();

        size = tagPoolMemory.size();
        for (uint32_t i = 0; i < size; ++i) {
            delete[] tagPoolMemory[i];
        }
        tagPoolMemory.clear();
    }

    NodeType *getTag() {
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
        size_t tagSize = sizeof(TagType);
        tagSize = alignUp(tagSize, tagAlignment);
        size_t allocationSizeRequired = tagCount * tagSize;

        GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemory(allocationSizeRequired);
        gfxAllocations.push_back(graphicsAllocation);

        uintptr_t Size = graphicsAllocation->getUnderlyingBufferSize();
        uintptr_t Start = reinterpret_cast<uintptr_t>(graphicsAllocation->getUnderlyingBuffer());
        uintptr_t End = Start + Size;
        size_t nodeCount = Size / tagSize;

        NodeType *nodesMemory = new NodeType[nodeCount];

        for (size_t i = 0; i < nodeCount; ++i) {
            nodesMemory[i].gfxAllocation = graphicsAllocation;
            nodesMemory[i].tag = reinterpret_cast<TagType *>(Start);
            freeTags.pushTailOne(nodesMemory[i]);
            Start += tagSize;
        }
        DEBUG_BREAK_IF(Start > End);
        ((void)(End));
        tagPoolMemory.push_back(nodesMemory);
    }
};
} // namespace OCLRT

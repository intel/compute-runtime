/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/idlist.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace NEO {
class GraphicsAllocation;

template <typename TagType>
class TagAllocator;

template <typename TagType>
struct TagNode : public IDNode<TagNode<TagType>>, NonCopyableOrMovableClass {
  public:
    TagType *tagForCpuAccess;

    GraphicsAllocation *getBaseGraphicsAllocation() const { return gfxAllocation; }
    uint64_t getGpuAddress() const { return gpuAddress; }

    void incRefCount() { refCount++; }

    MOCKABLE_VIRTUAL void returnTag() {
        allocator->returnTag(this);
    }

    bool canBeReleased() const {
        return (!doNotReleaseNodes) &&
               (tagForCpuAccess->isCompleted()) &&
               (tagForCpuAccess->getImplicitGpuDependenciesCount() == getImplicitCpuDependenciesCount());
    }

    void setDoNotReleaseNodes(bool doNotRelease) {
        doNotReleaseNodes = doNotRelease;
    }

    void setProfilingCapable(bool capable) { profilingCapable = capable; }

    bool isProfilingCapable() const { return profilingCapable; }

    void incImplicitCpuDependenciesCount() { implicitCpuDependenciesCount++; }

    void initialize() {
        tagForCpuAccess->initialize();
        implicitCpuDependenciesCount.store(0);
        setProfilingCapable(true);
    }

    uint32_t getImplicitCpuDependenciesCount() const { return implicitCpuDependenciesCount.load(); }

    const TagAllocator<TagType> *getAllocator() const { return allocator; }

  protected:
    TagAllocator<TagType> *allocator = nullptr;
    GraphicsAllocation *gfxAllocation = nullptr;
    uint64_t gpuAddress = 0;
    std::atomic<uint32_t> refCount{0};
    std::atomic<uint32_t> implicitCpuDependenciesCount{0};
    bool doNotReleaseNodes = false;
    bool profilingCapable = true;

    template <typename TagType2>
    friend class TagAllocator;
};

template <typename TagType>
class TagAllocator {
  public:
    using NodeType = TagNode<TagType>;

    TagAllocator(uint32_t rootDeviceIndex, MemoryManager *memMngr, size_t tagCount,
                 size_t tagAlignment, size_t tagSize, bool doNotReleaseNodes,
                 DeviceBitfield deviceBitfield) : deviceBitfield(deviceBitfield),
                                                  rootDeviceIndex(rootDeviceIndex),
                                                  memoryManager(memMngr),
                                                  tagCount(tagCount),
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
        node->initialize();
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
    std::vector<std::unique_ptr<NodeType[]>> tagPoolMemory;

    const DeviceBitfield deviceBitfield;
    const uint32_t rootDeviceIndex;
    MemoryManager *memoryManager;
    size_t tagCount;
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
        AllocationProperties allocationProperties{rootDeviceIndex, allocationSizeRequired, allocationType, deviceBitfield};
        GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
        gfxAllocations.push_back(graphicsAllocation);

        auto nodesMemory = std::make_unique<NodeType[]>(tagCount);

        for (size_t i = 0; i < tagCount; ++i) {
            auto tagOffset = i * tagSize;

            nodesMemory[i].allocator = this;
            nodesMemory[i].gfxAllocation = graphicsAllocation;
            nodesMemory[i].tagForCpuAccess = reinterpret_cast<TagType *>(ptrOffset(graphicsAllocation->getUnderlyingBuffer(), tagOffset));
            nodesMemory[i].gpuAddress = graphicsAllocation->getGpuAddress() + tagOffset;
            nodesMemory[i].setDoNotReleaseNodes(doNotReleaseNodes);

            freeTags.pushTailOne(nodesMemory[i]);
        }

        tagPoolMemory.push_back(std::move(nodesMemory));
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

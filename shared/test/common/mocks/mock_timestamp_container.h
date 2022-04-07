/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

namespace NEO {

template <typename TagType = TimestampPackets<uint32_t>>
class MockTagAllocator : public TagAllocator<TagType> {
  public:
    using BaseClass = TagAllocator<TagType>;
    using BaseClass::freeTags;
    using BaseClass::usedTags;
    using NodeType = typename BaseClass::NodeType;

    MockTagAllocator(uint32_t rootDeviceIndex, MemoryManager *memoryManager, size_t tagCount,
                     size_t tagAlignment, size_t tagSize, bool doNotReleaseNodes, DeviceBitfield deviceBitfield)
        : BaseClass(RootDeviceIndicesContainer({rootDeviceIndex}), memoryManager, tagCount, tagAlignment, tagSize, doNotReleaseNodes, deviceBitfield) {
    }

    MockTagAllocator(uint32_t rootDeviceIndex, MemoryManager *memoryManager, size_t tagCount = 10)
        : MockTagAllocator(rootDeviceIndex, memoryManager, tagCount, MemoryConstants::cacheLineSize, sizeof(TagType), false, mockDeviceBitfield) {
    }

    MockTagAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memoryManager, size_t tagCount = 10)
        : BaseClass(rootDeviceIndices, memoryManager, tagCount, MemoryConstants::cacheLineSize, sizeof(TagType), false, mockDeviceBitfield) {}

    void returnTag(TagNodeBase *node) override {
        releaseReferenceNodes.push_back(static_cast<NodeType *>(node));
        BaseClass::returnTag(node);
    }

    void returnTagToFreePool(TagNodeBase *node) override {
        returnedToFreePoolNodes.push_back(static_cast<NodeType *>(node));
        BaseClass::returnTagToFreePool(node);
    }

    std::vector<NodeType *> releaseReferenceNodes;
    std::vector<NodeType *> returnedToFreePoolNodes;
};

class MockTimestampPacketContainer : public TimestampPacketContainer {
  public:
    using TimestampPacketContainer::timestampPacketNodes;

    MockTimestampPacketContainer(TagAllocatorBase &tagAllocator, size_t numberOfPreallocatedTags) {
        for (size_t i = 0; i < numberOfPreallocatedTags; i++) {
            add(tagAllocator.getTag());
        }
    }

    TagNodeBase *getNode(size_t position) {
        return timestampPacketNodes.at(position);
    }
};
} // namespace NEO

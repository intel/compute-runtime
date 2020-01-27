/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/timestamp_packet.h"

namespace NEO {

template <typename TagType = TimestampPacketStorage>
class MockTagAllocator : public TagAllocator<TagType> {
  public:
    using BaseClass = TagAllocator<TagType>;
    using BaseClass::freeTags;
    using BaseClass::usedTags;
    using NodeType = typename BaseClass::NodeType;

    MockTagAllocator(uint32_t rootDeviceIndex, MemoryManager *memoryManager, size_t tagCount = 10)
        : BaseClass(rootDeviceIndex, memoryManager, tagCount, MemoryConstants::cacheLineSize, sizeof(TagType), false) {}

    void returnTag(NodeType *node) override {
        releaseReferenceNodes.push_back(node);
        BaseClass::returnTag(node);
    }

    void returnTagToFreePool(NodeType *node) override {
        returnedToFreePoolNodes.push_back(node);
        BaseClass::returnTagToFreePool(node);
    }

    std::vector<NodeType *> releaseReferenceNodes;
    std::vector<NodeType *> returnedToFreePoolNodes;
};

class MockTimestampPacketContainer : public TimestampPacketContainer {
  public:
    using TimestampPacketContainer::timestampPacketNodes;

    MockTimestampPacketContainer(TagAllocator<TimestampPacketStorage> &tagAllocator, size_t numberOfPreallocatedTags) {
        for (size_t i = 0; i < numberOfPreallocatedTags; i++) {
            add(tagAllocator.getTag());
        }
    }

    TagNode<TimestampPacketStorage> *getNode(size_t position) {
        return timestampPacketNodes.at(position);
    }
};
} // namespace NEO

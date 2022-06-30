/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/timestamp_packet.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/utilities/tag_allocator.h"

using namespace NEO;

void TimestampPacketContainer::add(TagNodeBase *timestampPacketNode) {
    timestampPacketNodes.push_back(timestampPacketNode);
}

TimestampPacketContainer::~TimestampPacketContainer() {
    for (auto node : timestampPacketNodes) {
        node->returnTag();
    }
}

void TimestampPacketContainer::swapNodes(TimestampPacketContainer &timestampPacketContainer) {
    timestampPacketNodes.swap(timestampPacketContainer.timestampPacketNodes);
}

void TimestampPacketContainer::assignAndIncrementNodesRefCounts(const TimestampPacketContainer &inputTimestampPacketContainer) {
    auto &inputNodes = inputTimestampPacketContainer.peekNodes();
    std::copy(inputNodes.begin(), inputNodes.end(), std::back_inserter(timestampPacketNodes));

    for (auto node : inputNodes) {
        node->incRefCount();
    }
}

void TimestampPacketContainer::makeResident(CommandStreamReceiver &commandStreamReceiver) {
    for (auto node : timestampPacketNodes) {
        commandStreamReceiver.makeResident(*node->getBaseGraphicsAllocation());
    }
}

void TimestampPacketContainer::moveNodesToNewContainer(TimestampPacketContainer &timestampPacketContainer) {
    TimestampPacketContainer tempContainer;
    swapNodes(tempContainer);

    timestampPacketContainer.assignAndIncrementNodesRefCounts(tempContainer);
}

void TimestampPacketDependencies::moveNodesToNewContainer(TimestampPacketContainer &timestampPacketContainer) {
    cacheFlushNodes.moveNodesToNewContainer(timestampPacketContainer);
    previousEnqueueNodes.moveNodesToNewContainer(timestampPacketContainer);
    barrierNodes.moveNodesToNewContainer(timestampPacketContainer);
    auxToNonAuxNodes.moveNodesToNewContainer(timestampPacketContainer);
    nonAuxToAuxNodes.moveNodesToNewContainer(timestampPacketContainer);
}

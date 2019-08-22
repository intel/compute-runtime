/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/timestamp_packet.h"

#include "core/command_stream/linear_stream.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/event.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/utilities/tag_allocator.h"

using namespace NEO;

void TimestampPacketContainer::add(Node *timestampPacketNode) {
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

void TimestampPacketContainer::resolveDependencies(bool clearAllDependencies) {
    std::vector<Node *> pendingNodes;

    for (auto node : timestampPacketNodes) {
        if (node->tagForCpuAccess->canBeReleased() || clearAllDependencies) {
            node->returnTag();
        } else {
            pendingNodes.push_back(node);
        }
    }

    std::swap(timestampPacketNodes, pendingNodes);
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

bool TimestampPacketContainer::isCompleted() const {
    for (auto node : timestampPacketNodes) {
        if (!node->tagForCpuAccess->isCompleted()) {
            return false;
        }
    }
    return true;
}

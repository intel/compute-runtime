/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/stackvec.h"

namespace NEO {

class CommandStreamReceiver;
class TagNodeBase;

class TimestampPacketContainer : public NonCopyableClass {
  public:
    TimestampPacketContainer() = default;
    TimestampPacketContainer(TimestampPacketContainer &&) = default;
    TimestampPacketContainer &operator=(TimestampPacketContainer &&) = default;
    MOCKABLE_VIRTUAL ~TimestampPacketContainer();

    const StackVec<TagNodeBase *, 32u> &peekNodes() const { return timestampPacketNodes; }
    void add(TagNodeBase *timestampPacketNode);
    void swapNodes(TimestampPacketContainer &timestampPacketContainer);
    void assignAndIncrementNodesRefCounts(const TimestampPacketContainer &inputTimestampPacketContainer);
    void makeResident(CommandStreamReceiver &commandStreamReceiver);
    void moveNodesToNewContainer(TimestampPacketContainer &timestampPacketContainer);
    void releaseNodes();

  protected:
    StackVec<TagNodeBase *, 32u> timestampPacketNodes;
};

struct TimestampPacketDependencies : public NonCopyableClass {
    TimestampPacketContainer cacheFlushNodes;
    TimestampPacketContainer previousEnqueueNodes;
    TimestampPacketContainer barrierNodes;
    TimestampPacketContainer auxToNonAuxNodes;
    TimestampPacketContainer nonAuxToAuxNodes;
    TimestampPacketContainer multiCsrDependencies;

    void moveNodesToNewContainer(TimestampPacketContainer &timestampPacketContainer);
};

} // namespace NEO
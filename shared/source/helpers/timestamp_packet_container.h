/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

namespace NEO {

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

  protected:
    StackVec<TagNodeBase *, 32u> timestampPacketNodes;
};

} // namespace NEO
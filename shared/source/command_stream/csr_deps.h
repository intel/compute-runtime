/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include <set>

namespace NEO {

class TimestampPacketContainer;
class CommandStreamReceiver;

class CsrDependencies {
  public:
    enum class DependenciesType {
        onCsr,
        outOfCsr,
        all
    };

    StackVec<TimestampPacketContainer *, 32> multiRootTimeStampSyncContainer;
    StackVec<TimestampPacketContainer *, 32> timestampPacketContainer;

    void makeResident(CommandStreamReceiver &commandStreamReceiver) const;
    void copyNodesToNewContainer(TimestampPacketContainer &newTimestampPacketContainer);
    void copyRootDeviceSyncNodesToNewContainer(TimestampPacketContainer &newTimestampPacketContainer);

    std::set<CommandStreamReceiver *> csrWithMultiEngineDependencies;
    bool containsCrossEngineDependency = false;
};
} // namespace NEO

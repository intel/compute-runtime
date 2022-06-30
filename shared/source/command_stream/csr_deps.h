/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

namespace NEO {

class TimestampPacketContainer;
class CommandStreamReceiver;

class CsrDependencies {
  public:
    enum class DependenciesType {
        OnCsr,
        OutOfCsr,
        All
    };

    StackVec<std::pair<uint32_t, uint64_t>, 32> taskCountContainer;
    StackVec<TimestampPacketContainer *, 32> timestampPacketContainer;

    void makeResident(CommandStreamReceiver &commandStreamReceiver) const;
    void copyNodesToNewContainer(TimestampPacketContainer &newTimestampPacketContainer);
};
} // namespace NEO

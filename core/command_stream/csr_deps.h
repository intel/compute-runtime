/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/utilities/stackvec.h"

namespace NEO {

class TimestampPacketContainer;
class CommandStreamReceiver;

class CsrDependencies : public StackVec<TimestampPacketContainer *, 32> {
  public:
    enum class DependenciesType {
        OnCsr,
        OutOfCsr,
        All
    };

    void makeResident(CommandStreamReceiver &commandStreamReceiver) const;
};
} // namespace NEO

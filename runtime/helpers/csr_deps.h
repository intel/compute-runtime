/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/utilities/stackvec.h"

namespace NEO {

class TimestampPacketContainer;
class CommandStreamReceiver;
struct EventsRequest;

class CsrDependencies : public StackVec<TimestampPacketContainer *, 32> {
  public:
    enum class DependenciesType {
        OnCsr,
        OutOfCsr,
        All
    };

    void fillFromEventsRequest(const EventsRequest &eventsRequest, CommandStreamReceiver &currentCsr,
                               DependenciesType depsType);

    void makeResident(CommandStreamReceiver &commandStreamReceiver) const;
};
} // namespace NEO

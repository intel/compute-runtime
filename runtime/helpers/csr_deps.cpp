/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/csr_deps.h"

#include "runtime/command_queue/command_queue.h"
#include "runtime/event/event.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/helpers/timestamp_packet.h"

namespace NEO {
void CsrDependencies::fillFromEventsRequest(const EventsRequest &eventsRequest, CommandStreamReceiver &currentCsr,
                                            DependenciesType depsType) {
    for (cl_uint i = 0; i < eventsRequest.numEventsInWaitList; i++) {
        auto event = castToObjectOrAbort<Event>(eventsRequest.eventWaitList[i]);
        if (event->isUserEvent()) {
            continue;
        }

        auto timestampPacketContainer = event->getTimestampPacketNodes();
        if (!timestampPacketContainer || timestampPacketContainer->peekNodes().empty()) {
            continue;
        }

        auto sameCsr = (&event->getCommandQueue()->getGpgpuCommandStreamReceiver() == &currentCsr);
        bool pushDependency = (DependenciesType::OnCsr == depsType && sameCsr) ||
                              (DependenciesType::OutOfCsr == depsType && !sameCsr) ||
                              (DependenciesType::All == depsType);

        if (pushDependency) {
            this->push_back(timestampPacketContainer);
        }
    }
}

void CsrDependencies::makeResident(CommandStreamReceiver &commandStreamReceiver) const {
    for (auto &timestampPacketContainer : *this) {
        timestampPacketContainer->makeResident(commandStreamReceiver);
    }
}
} // namespace NEO

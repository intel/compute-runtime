/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/event/event.h"
#include "runtime/helpers/csr_deps.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/helpers/timestamp_packet.h"

namespace OCLRT {
void CsrDependencies::fillFromEventsRequestAndMakeResident(const EventsRequest &eventsRequest,
                                                           CommandStreamReceiver &currentCsr,
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

        timestampPacketContainer->makeResident(currentCsr);
        auto sameCsr = (&event->getCommandQueue()->getCommandStreamReceiver() == &currentCsr);

        if (depsType == (sameCsr ? DependenciesType::OnCsr : DependenciesType::OutOfCsr)) {
            this->push_back(timestampPacketContainer);
        }
    }
}
} // namespace OCLRT

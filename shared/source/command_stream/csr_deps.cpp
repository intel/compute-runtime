/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_deps.h"

#include "shared/source/helpers/timestamp_packet.h"

namespace NEO {

void CsrDependencies::makeResident(CommandStreamReceiver &commandStreamReceiver) const {
    for (auto &timestampPacketContainer : timestampPacketContainer) {
        timestampPacketContainer->makeResident(commandStreamReceiver);
    }
    for (auto &timestampPacketContainer : multiRootTimeStampSyncContainer) {
        timestampPacketContainer->makeResident(commandStreamReceiver);
    }
}

void CsrDependencies::copyNodesToNewContainer(TimestampPacketContainer &newTimestampPacketContainer) {
    for (auto &timestampPacketContainer : timestampPacketContainer) {
        newTimestampPacketContainer.assignAndIncrementNodesRefCounts(*timestampPacketContainer);
    }
}
void CsrDependencies::copyRootDeviceSyncNodesToNewContainer(TimestampPacketContainer &newTimestampPacketContainer) {
    for (auto &timestampPacketContainer : multiRootTimeStampSyncContainer) {
        newTimestampPacketContainer.assignAndIncrementNodesRefCounts(*timestampPacketContainer);
    }
}

} // namespace NEO

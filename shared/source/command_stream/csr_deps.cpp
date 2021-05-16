/*
 * Copyright (C) 2020-2021 Intel Corporation
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
}
} // namespace NEO

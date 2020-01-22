/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/csr_deps.h"

#include "runtime/helpers/timestamp_packet.h"

namespace NEO {

void CsrDependencies::makeResident(CommandStreamReceiver &commandStreamReceiver) const {
    for (auto &timestampPacketContainer : *this) {
        timestampPacketContainer->makeResident(commandStreamReceiver);
    }
}
} // namespace NEO

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/timestamp_packet.h"

namespace NEO {

class MockTimestampPackets32 : public TimestampPackets<uint32_t> {
  public:
    using typename TimestampPackets<uint32_t>::Packet;
    using TimestampPackets<uint32_t>::packets;

    void setTagToReadyState() {
        initialize();

        uint32_t zeros[4] = {};

        for (uint32_t i = 0; i < TimestampPacketSizeControl::preferredPacketCount; i++) {
            assignDataToAllTimestamps(i, zeros);
        }
    }

    void setToNonReadyState() {
        packets[0].contextEnd = 1;
    }
};

} // namespace NEO

/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"

struct CommandStreamReceiverFixture : public NEO::DeviceFixture {
    void setUp();
    void tearDown();

    static constexpr uint64_t cmdBufferGpuAddress = 0x10000;
    static constexpr size_t bufferSize = 1024;
    uint8_t cmdBuffer[bufferSize];
    uint8_t dshBuffer[bufferSize];
    uint8_t iohBuffer[bufferSize];
    uint8_t sshBuffer[bufferSize];

    NEO::LinearStream commandStream;
    NEO::IndirectHeap dsh = {nullptr};
    NEO::IndirectHeap ioh = {nullptr};
    NEO::IndirectHeap ssh = {nullptr};

    NEO::StreamProperties requiredStreamProperties = {};
    NEO::DispatchFlags flushTaskFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    NEO::ImmediateDispatchFlags immediateFlushTaskFlags = {};

    uint32_t taskLevel = 2;
};

struct CommandStreamReceiverSystolicFixture : public CommandStreamReceiverFixture {
    template <typename FamilyType>
    void testBody();
};

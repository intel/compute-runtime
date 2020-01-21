/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "unit_tests/command_stream/command_stream_fixture.h"

#include <cstdint>

namespace NEO {

class CommandStreamReceiver;
class MockDevice;

class TbxCommandStreamFixture : public CommandStreamFixture {
  public:
    virtual void SetUp(MockDevice *pDevice);
    virtual void TearDown(void);

    CommandStreamReceiver *pCommandStreamReceiver = nullptr;

    MemoryManager *memoryManager;
};
} // namespace NEO

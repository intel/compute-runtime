/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"

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

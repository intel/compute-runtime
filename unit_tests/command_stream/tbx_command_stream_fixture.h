/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "unit_tests/command_stream/command_stream_fixture.h"

#include <cstdint>

namespace OCLRT {

class CommandStreamReceiver;
class MockDevice;

class TbxCommandStreamFixture : public CommandStreamFixture {
  public:
    virtual void SetUp(MockDevice *pDevice);
    virtual void TearDown(void);

    CommandStreamReceiver *pCommandStreamReceiver = nullptr;

    MemoryManager *mmTbx;
};
} // namespace OCLRT

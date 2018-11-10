/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/tbx_command_stream_receiver.h"

namespace OCLRT {

struct MockTbxStream : public TbxCommandStreamReceiver::TbxStream {
    using TbxCommandStreamReceiver::TbxStream::socket;
};
} // namespace OCLRT

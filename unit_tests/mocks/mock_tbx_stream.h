/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/tbx_command_stream_receiver.h"

namespace NEO {

struct MockTbxStream : public TbxCommandStreamReceiver::TbxStream {
    using TbxCommandStreamReceiver::TbxStream::socket;
};
} // namespace NEO

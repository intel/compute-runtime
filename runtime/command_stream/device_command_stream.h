/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver_hw.h"

namespace OCLRT {

template <typename GfxFamily>
class DeviceCommandStreamReceiver : public CommandStreamReceiverHw<GfxFamily> {
    typedef CommandStreamReceiverHw<GfxFamily> BaseClass;

  protected:
    DeviceCommandStreamReceiver(ExecutionEnvironment &executionEnvironment)
        : BaseClass(executionEnvironment) {
    }

  public:
    static CommandStreamReceiver *create(bool withAubDump, ExecutionEnvironment &executionEnvironment);
};
} // namespace OCLRT

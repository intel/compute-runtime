/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver_hw.h"

namespace OCLRT {
struct HardwareInfo;

template <typename GfxFamily>
class DeviceCommandStreamReceiver : public CommandStreamReceiverHw<GfxFamily> {
    typedef CommandStreamReceiverHw<GfxFamily> BaseClass;

  protected:
    DeviceCommandStreamReceiver(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment)
        : BaseClass(hwInfoIn, executionEnvironment) {
    }

  public:
    static CommandStreamReceiver *create(const HardwareInfo &hwInfo, bool withAubDump, ExecutionEnvironment &executionEnvironment);
};
} // namespace OCLRT

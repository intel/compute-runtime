/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver_hw.h"

namespace NEO {

template <typename GfxFamily>
class DeviceCommandStreamReceiver : public CommandStreamReceiverHw<GfxFamily> {
    typedef CommandStreamReceiverHw<GfxFamily> BaseClass;

  protected:
    DeviceCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex)
        : BaseClass(executionEnvironment, rootDeviceIndex) {
    }

  public:
    static CommandStreamReceiver *create(bool withAubDump, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
};
} // namespace NEO

/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_hw.h"

namespace NEO {

template <typename GfxFamily>
CommandStreamReceiver *createDeviceCommandStreamReceiver(bool withAubDump,
                                                         ExecutionEnvironment &executionEnvironment,
                                                         uint32_t rootDeviceIndex,
                                                         const DeviceBitfield deviceBitfield);

template <typename GfxFamily>
class DeviceCommandStreamReceiver : public CommandStreamReceiverHw<GfxFamily> {
    typedef CommandStreamReceiverHw<GfxFamily> BaseClass;

  protected:
    DeviceCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                                uint32_t rootDeviceIndex,
                                const DeviceBitfield deviceBitfield)
        : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield) {
    }

  public:
    static CommandStreamReceiver *create(bool withAubDump,
                                         ExecutionEnvironment &executionEnvironment,
                                         uint32_t rootDeviceIndex,
                                         const DeviceBitfield deviceBitfield) {
        return createDeviceCommandStreamReceiver<GfxFamily>(withAubDump, executionEnvironment,
                                                            rootDeviceIndex, deviceBitfield);
    }
};
} // namespace NEO

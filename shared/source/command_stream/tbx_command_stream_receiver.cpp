/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/tbx_command_stream_receiver.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"

#include <string>

namespace NEO {

TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_CORE] = {};

CommandStreamReceiver *TbxCommandStreamReceiver::create(const std::string &baseName,
                                                        bool withAubDump,
                                                        ExecutionEnvironment &executionEnvironment,
                                                        uint32_t rootDeviceIndex,
                                                        const DeviceBitfield deviceBitfield) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();

    if (hwInfo->platform.eRenderCoreFamily >= IGFX_MAX_CORE) {
        DEBUG_BREAK_IF(!false);
        return nullptr;
    }

    auto pCreate = tbxCommandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily];

    return pCreate ? pCreate(baseName, withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield) : nullptr;
}
} // namespace NEO

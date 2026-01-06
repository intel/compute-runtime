/*
 * Copyright (C) 2018-2025 Intel Corporation
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

TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[NEO::maxCoreEnumValue] = {};

CommandStreamReceiver *TbxCommandStreamReceiver::create(const std::string &baseName,
                                                        bool withAubDump,
                                                        ExecutionEnvironment &executionEnvironment,
                                                        uint32_t rootDeviceIndex,
                                                        const DeviceBitfield deviceBitfield) {
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();

    if (hwInfo->platform.eRenderCoreFamily >= NEO::maxCoreEnumValue) {
        DEBUG_BREAK_IF(!false);
        return nullptr;
    }

    auto pCreate = tbxCommandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily];

    return pCreate ? pCreate(baseName, withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield) : nullptr;
}
} // namespace NEO

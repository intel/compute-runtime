/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/tbx_command_stream_receiver.h"

#include "core/execution_environment/execution_environment.h"
#include "core/helpers/hw_info.h"

#include <string>

namespace NEO {

TbxCommandStreamReceiverCreateFunc tbxCommandStreamReceiverFactory[IGFX_MAX_CORE] = {};

CommandStreamReceiver *TbxCommandStreamReceiver::create(const std::string &baseName, bool withAubDump, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) {
    auto hwInfo = executionEnvironment.getHardwareInfo();

    if (hwInfo->platform.eRenderCoreFamily >= IGFX_MAX_CORE) {
        DEBUG_BREAK_IF(!false);
        return nullptr;
    }

    auto pCreate = tbxCommandStreamReceiverFactory[hwInfo->platform.eRenderCoreFamily];

    return pCreate ? pCreate(baseName, withAubDump, executionEnvironment, rootDeviceIndex) : nullptr;
}
} // namespace NEO

/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

namespace NEO {
class CommandStreamReceiver;
class TbxSockets;
class ExecutionEnvironment;

struct TbxCommandStreamReceiver {
    static CommandStreamReceiver *create(const std::string &baseName,
                                         bool withAubDump,
                                         ExecutionEnvironment &executionEnvironment,
                                         uint32_t rootDeviceIndex,
                                         const DeviceBitfield deviceBitfield);
};

typedef CommandStreamReceiver *(*TbxCommandStreamReceiverCreateFunc)(const std::string &baseName,
                                                                     bool withAubDump,
                                                                     ExecutionEnvironment &executionEnvironment,
                                                                     uint32_t rootDeviceIndex,
                                                                     const DeviceBitfield deviceBitfield);
} // namespace NEO

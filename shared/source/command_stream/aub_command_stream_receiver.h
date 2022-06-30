/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/helpers/common_types.h"

#include <string>

namespace NEO {
struct HardwareInfo;
class CommandStreamReceiver;
class ExecutionEnvironment;

struct AUBCommandStreamReceiver {
    static CommandStreamReceiver *create(const std::string &filename,
                                         bool standalone,
                                         ExecutionEnvironment &executionEnvironment,
                                         uint32_t rootDeviceIndex,
                                         const DeviceBitfield deviceBitfield);
    static std::string createFullFilePath(const HardwareInfo &hwInfo, const std::string &filename, uint32_t rootDeviceIndex);

    using AubFileStream = AubMemDump::AubFileStream;
};

typedef CommandStreamReceiver *(*AubCommandStreamReceiverCreateFunc)(const std::string &fileName,
                                                                     bool standalone,
                                                                     ExecutionEnvironment &executionEnvironment,
                                                                     uint32_t rootDeviceIndex,
                                                                     const DeviceBitfield deviceBitfield);
} // namespace NEO

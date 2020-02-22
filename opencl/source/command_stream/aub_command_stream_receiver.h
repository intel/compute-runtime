/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/aub_mem_dump/aub_mem_dump.h"

#include <string>

namespace NEO {
struct HardwareInfo;
class CommandStreamReceiver;
class ExecutionEnvironment;

struct AUBCommandStreamReceiver {
    static CommandStreamReceiver *create(const std::string &filename, bool standalone, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
    static std::string createFullFilePath(const HardwareInfo &hwInfo, const std::string &filename);

    using AubFileStream = AubMemDump::AubFileStream;
};

typedef CommandStreamReceiver *(*AubCommandStreamReceiverCreateFunc)(const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
} // namespace NEO

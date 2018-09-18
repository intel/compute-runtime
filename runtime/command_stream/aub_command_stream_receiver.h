/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include <string>

namespace OCLRT {
struct HardwareInfo;
class CommandStreamReceiver;
class ExecutionEnvironment;

struct AUBCommandStreamReceiver {
    static CommandStreamReceiver *create(const HardwareInfo &hwInfo, const std::string &filename, bool standalone, ExecutionEnvironment &executionEnvironment);

    using AubFileStream = AubMemDump::AubFileStream;
};

typedef CommandStreamReceiver *(*AubCommandStreamReceiverCreateFunc)(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment);
} // namespace OCLRT

/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/device_bitfield.h"

#include <cstdint>
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
    static std::string getDirectoryPathForFilePath(const std::string &filePath);
    static void createDirectoriesForFilePath(const std::string &filePath);
};

typedef CommandStreamReceiver *(*AubCommandStreamReceiverCreateFunc)(const std::string &fileName,
                                                                     bool standalone,
                                                                     ExecutionEnvironment &executionEnvironment,
                                                                     uint32_t rootDeviceIndex,
                                                                     const DeviceBitfield deviceBitfield);
} // namespace NEO

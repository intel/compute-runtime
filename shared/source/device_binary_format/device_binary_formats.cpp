/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"

namespace NEO {

std::vector<uint8_t> packDeviceBinary(const SingleDeviceBinary binary, std::string &outErrReason, std::string &outWarning) {
    if (NEO::isAnyPackedDeviceBinaryFormat(binary.deviceBinary)) {
        return std::vector<uint8_t>(binary.deviceBinary.begin(), binary.deviceBinary.end());
    }
    return packDeviceBinary<DeviceBinaryFormat::OclElf>(binary, outErrReason, outWarning);
}

} // namespace NEO

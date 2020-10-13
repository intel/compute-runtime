/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/built_ins_helper.h"

#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"

#include "opencl/source/program/kernel_info.h"

namespace NEO {
const SipKernel &initSipKernel(SipKernelType type, Device &device) {
    return device.getBuiltIns()->getSipKernel(type, device);
}
ProgramInfo createProgramInfoForSip(std::vector<char> &binary, size_t size, const Device &device) {

    ProgramInfo programInfo;
    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size());
    SingleDeviceBinary deviceBinary = {};
    deviceBinary.deviceBinary = blob;
    std::string decodeErrors;
    std::string decodeWarnings;

    DecodeError decodeError;
    DeviceBinaryFormat singleDeviceBinaryFormat;
    std::tie(decodeError, singleDeviceBinaryFormat) = NEO::decodeSingleDeviceBinary(programInfo, deviceBinary, decodeErrors, decodeWarnings);
    UNRECOVERABLE_IF(DecodeError::Success != decodeError);

    auto success = programInfo.kernelInfos[0]->createKernelAllocation(device);
    UNRECOVERABLE_IF(!success);

    return programInfo;
}
} // namespace NEO

/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/device_binary_format/patchtokens_dumper.h"
#include "shared/source/device_binary_format/patchtokens_validator.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/program/program_info_from_patchtokens.h"

#include "opencl/source/utilities/logger.h"

namespace NEO {

template <>
bool isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Patchtokens>(const ArrayRef<const uint8_t> binary) {
    return NEO::PatchTokenBinary::isValidPatchTokenBinary(binary);
}

template <>
SingleDeviceBinary unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                                                                  std::string &outErrReason, std::string &outWarning) {
    auto programHeader = NEO::PatchTokenBinary::decodeProgramHeader(archive);
    if (nullptr == programHeader) {
        outErrReason = "Invalid program header";
        return {};
    }
    bool validForTarget = (requestedTargetDevice.coreFamily == static_cast<GFXCORE_FAMILY>(programHeader->Device));
    validForTarget &= (requestedTargetDevice.maxPointerSizeInBytes >= programHeader->GPUPointerSizeInBytes);
    validForTarget &= (iOpenCL::CURRENT_ICBE_VERSION == programHeader->Version);
    if (false == validForTarget) {
        outErrReason = "Unhandled target device";
        return {};
    }
    SingleDeviceBinary ret = {};
    ret.targetDevice = requestedTargetDevice;
    ret.format = NEO::DeviceBinaryFormat::Patchtokens;
    ret.deviceBinary = archive;
    return ret;
}

template <>
DecodeError decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning) {
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram = {};
    NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(src.deviceBinary, decodedProgram);
    DBG_LOG(LogPatchTokens, NEO::PatchTokenBinary::asString(decodedProgram).c_str());

    std::string validatorWarnings;
    std::string validatorErrMessage;
    auto validatorErr = PatchTokenBinary::validate(decodedProgram, outErrReason, outWarning);
    if (DecodeError::Success != validatorErr) {
        return validatorErr;
    }

    NEO::populateProgramInfo(dst, decodedProgram);

    return DecodeError::Success;
}

} // namespace NEO

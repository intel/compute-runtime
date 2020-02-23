/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include <cstddef>
#include <cstdint>
#include <igfxfmid.h>
#include <memory>
#include <vector>

namespace NEO {
struct ProgramInfo;

enum class DeviceBinaryFormat : uint8_t {
    Unknown,
    OclElf,
    OclLibrary,
    OclCompiledObject,
    Patchtokens,
    Archive
};

enum class DecodeError : uint8_t {
    Success,
    Undefined,
    InvalidBinary,
    UnhandledBinary
};

inline const char *asString(DecodeError err) {
    switch (err) {
    default:
        return "with invalid binary";
        break;
    case DecodeError::UnhandledBinary:
        return "with unhandled binary";
        break;
    case DecodeError::Success:
        return "decoded successfully";
        break;
    case DecodeError::Undefined:
        return "in undefined status";
        break;
    }
}

struct TargetDevice {
    GFXCORE_FAMILY coreFamily = IGFX_UNKNOWN_CORE;
    uint32_t stepping = 0U;
    uint32_t maxPointerSizeInBytes = 4U;
};

struct SingleDeviceBinary {
    DeviceBinaryFormat format = DeviceBinaryFormat::Unknown;
    ArrayRef<const uint8_t> deviceBinary;
    ArrayRef<const uint8_t> debugData;
    ArrayRef<const uint8_t> intermediateRepresentation;
    ConstStringRef buildOptions;
    TargetDevice targetDevice;
};

template <DeviceBinaryFormat Format>
bool isDeviceBinaryFormat(const ArrayRef<const uint8_t> binary);

template <>
bool isDeviceBinaryFormat<DeviceBinaryFormat::OclElf>(const ArrayRef<const uint8_t>);
template <>
bool isDeviceBinaryFormat<DeviceBinaryFormat::Patchtokens>(const ArrayRef<const uint8_t>);
template <>
bool isDeviceBinaryFormat<DeviceBinaryFormat::Archive>(const ArrayRef<const uint8_t>);

inline bool isAnyDeviceBinaryFormat(const ArrayRef<const uint8_t> binary) {
    if (isDeviceBinaryFormat<DeviceBinaryFormat::OclElf>(binary)) {
        return true;
    }
    if (isDeviceBinaryFormat<DeviceBinaryFormat::Patchtokens>(binary)) {
        return true;
    }
    if (isDeviceBinaryFormat<DeviceBinaryFormat::Archive>(binary)) {
        return true;
    }
    return false;
}

template <DeviceBinaryFormat Format>
SingleDeviceBinary unpackSingleDeviceBinary(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                            std::string &outErrReason, std::string &outWarning);

template <>
SingleDeviceBinary unpackSingleDeviceBinary<DeviceBinaryFormat::OclElf>(const ArrayRef<const uint8_t>, const ConstStringRef, const TargetDevice &, std::string &, std::string &);
template <>
SingleDeviceBinary unpackSingleDeviceBinary<DeviceBinaryFormat::Patchtokens>(const ArrayRef<const uint8_t>, const ConstStringRef, const TargetDevice &, std::string &, std::string &);
template <>
SingleDeviceBinary unpackSingleDeviceBinary<DeviceBinaryFormat::Archive>(const ArrayRef<const uint8_t>, const ConstStringRef, const TargetDevice &, std::string &, std::string &);

inline SingleDeviceBinary unpackSingleDeviceBinary(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                                   std::string &outErrReason, std::string &outWarning) {
    SingleDeviceBinary ret = {};
    ret.format = DeviceBinaryFormat::Unknown;
    if (isDeviceBinaryFormat<DeviceBinaryFormat::OclElf>(archive)) {
        return unpackSingleDeviceBinary<DeviceBinaryFormat::OclElf>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::Patchtokens>(archive)) {
        return unpackSingleDeviceBinary<DeviceBinaryFormat::Patchtokens>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::Archive>(archive)) {
        return unpackSingleDeviceBinary<DeviceBinaryFormat::Archive>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning);
    } else {
        outErrReason = "Unknown format";
    }
    return ret;
}

template <DeviceBinaryFormat Format>
std::vector<uint8_t> packDeviceBinary(const SingleDeviceBinary binary, std::string &outErrReason, std::string &outWarning);

template <>
std::vector<uint8_t> packDeviceBinary<DeviceBinaryFormat::OclElf>(const SingleDeviceBinary, std::string &, std::string &);

std::vector<uint8_t> packDeviceBinary(const SingleDeviceBinary binary, std::string &outErrReason, std::string &outWarning);

inline bool isAnyPackedDeviceBinaryFormat(const ArrayRef<const uint8_t> binary) {
    if (isDeviceBinaryFormat<DeviceBinaryFormat::OclElf>(binary)) {
        return true;
    }
    if (isDeviceBinaryFormat<DeviceBinaryFormat::Archive>(binary)) {
        return true;
    }
    return false;
}

inline bool isAnySingleDeviceBinaryFormat(const ArrayRef<const uint8_t> binary) {
    return (false == isAnyPackedDeviceBinaryFormat(binary)) && isAnyDeviceBinaryFormat(binary);
}

template <DeviceBinaryFormat Format>
DecodeError decodeSingleDeviceBinary(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning);

template <>
DecodeError decodeSingleDeviceBinary<DeviceBinaryFormat::OclElf>(ProgramInfo &, const SingleDeviceBinary &, std::string &, std::string &);
template <>
DecodeError decodeSingleDeviceBinary<DeviceBinaryFormat::Patchtokens>(ProgramInfo &, const SingleDeviceBinary &, std::string &, std::string &);
template <>
DecodeError decodeSingleDeviceBinary<DeviceBinaryFormat::Archive>(ProgramInfo &, const SingleDeviceBinary &, std::string &, std::string &);

inline std::pair<DecodeError, DeviceBinaryFormat> decodeSingleDeviceBinary(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning) {
    std::pair<DecodeError, DeviceBinaryFormat> ret;
    ret.first = DecodeError::InvalidBinary;
    ret.second = DeviceBinaryFormat::Unknown;
    if (isDeviceBinaryFormat<DeviceBinaryFormat::OclElf>(src.deviceBinary)) {
        ret.second = DeviceBinaryFormat::OclElf;
        ret.first = decodeSingleDeviceBinary<DeviceBinaryFormat::OclElf>(dst, src, outErrReason, outWarning);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::Patchtokens>(src.deviceBinary)) {
        ret.second = DeviceBinaryFormat::Patchtokens;
        ret.first = decodeSingleDeviceBinary<DeviceBinaryFormat::Patchtokens>(dst, src, outErrReason, outWarning);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::Archive>(src.deviceBinary)) {
        ret.second = DeviceBinaryFormat::Archive;
        ret.first = decodeSingleDeviceBinary<DeviceBinaryFormat::Archive>(dst, src, outErrReason, outWarning);
    } else {
        outErrReason = "Unknown format";
    }
    return ret;
}

} // namespace NEO

/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include <cstdint>
#include <igfxfmid.h>
#include <vector>

namespace NEO {
struct ProgramInfo;
struct RootDeviceEnvironment;
class GfxCoreHelper;

enum class DeviceBinaryFormat : uint8_t {
    unknown,
    oclElf,
    oclLibrary,
    oclCompiledObject,
    patchtokens,
    archive,
    zebin
};

enum class DecodeError : uint8_t {
    success,
    undefined,
    invalidBinary,
    unhandledBinary,
    unkownZeinfoAttribute
};

enum class GeneratorType : uint8_t {
    unknown,
    igc
};

inline const char *asString(DecodeError err) {
    switch (err) {
    default:
        return "with invalid binary";
        break;
    case DecodeError::unhandledBinary:
        return "with unhandled binary";
        break;
    case DecodeError::success:
        return "decoded successfully";
        break;
    case DecodeError::undefined:
        return "in undefined status";
        break;
    }
}

struct TargetDevice {
    GFXCORE_FAMILY coreFamily = IGFX_UNKNOWN_CORE;
    PRODUCT_FAMILY productFamily = IGFX_UNKNOWN;
    HardwareIpVersion aotConfig = {0};
    uint32_t stepping = 0U;
    uint32_t maxPointerSizeInBytes = 4U;
    uint32_t grfSize = 32U;
    uint32_t minScratchSpaceSize = 0U;
    uint32_t samplerStateSize = 0U;
    uint32_t samplerBorderColorStateSize = 0U;

    bool applyValidationWorkaround = false;
};
TargetDevice getTargetDevice(const RootDeviceEnvironment &rootDeviceEnvironment);

struct GeneratorFeatureVersions {
    using VersionT = uint32_t;
    VersionT indirectMemoryAccessDetection = 0u;
    VersionT indirectAccessBuffer = 0u;
};

struct SingleDeviceBinary {
    DeviceBinaryFormat format = DeviceBinaryFormat::unknown;
    ArrayRef<const uint8_t> deviceBinary;
    ArrayRef<const uint8_t> debugData;
    ArrayRef<const uint8_t> intermediateRepresentation;
    ArrayRef<const uint8_t> packedTargetDeviceBinary;
    ConstStringRef buildOptions;
    TargetDevice targetDevice;
    GeneratorType generator = GeneratorType::igc;
    GeneratorFeatureVersions generatorFeatureVersions;
};

template <DeviceBinaryFormat format>
bool isDeviceBinaryFormat(const ArrayRef<const uint8_t> binary);

template <>
bool isDeviceBinaryFormat<DeviceBinaryFormat::oclElf>(const ArrayRef<const uint8_t>);
template <>
bool isDeviceBinaryFormat<DeviceBinaryFormat::patchtokens>(const ArrayRef<const uint8_t>);
template <>
bool isDeviceBinaryFormat<DeviceBinaryFormat::archive>(const ArrayRef<const uint8_t>);
template <>
bool isDeviceBinaryFormat<DeviceBinaryFormat::zebin>(const ArrayRef<const uint8_t>);

inline bool isAnyDeviceBinaryFormat(const ArrayRef<const uint8_t> binary) {
    if (isDeviceBinaryFormat<DeviceBinaryFormat::oclElf>(binary)) {
        return true;
    }
    if (isDeviceBinaryFormat<DeviceBinaryFormat::patchtokens>(binary)) {
        return true;
    }
    if (isDeviceBinaryFormat<DeviceBinaryFormat::archive>(binary)) {
        return true;
    }
    if (isDeviceBinaryFormat<DeviceBinaryFormat::zebin>(binary)) {
        return true;
    }
    return false;
}

template <DeviceBinaryFormat format>
SingleDeviceBinary unpackSingleDeviceBinary(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                            std::string &outErrReason, std::string &outWarning);

template <>
SingleDeviceBinary unpackSingleDeviceBinary<DeviceBinaryFormat::oclElf>(const ArrayRef<const uint8_t>, const ConstStringRef, const TargetDevice &, std::string &, std::string &);
template <>
SingleDeviceBinary unpackSingleDeviceBinary<DeviceBinaryFormat::patchtokens>(const ArrayRef<const uint8_t>, const ConstStringRef, const TargetDevice &, std::string &, std::string &);
template <>
SingleDeviceBinary unpackSingleDeviceBinary<DeviceBinaryFormat::archive>(const ArrayRef<const uint8_t>, const ConstStringRef, const TargetDevice &, std::string &, std::string &);
template <>
SingleDeviceBinary unpackSingleDeviceBinary<DeviceBinaryFormat::zebin>(const ArrayRef<const uint8_t>, const ConstStringRef, const TargetDevice &, std::string &, std::string &);

inline SingleDeviceBinary unpackSingleDeviceBinary(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                                   std::string &outErrReason, std::string &outWarning) {
    SingleDeviceBinary ret = {};
    ret.format = DeviceBinaryFormat::unknown;
    if (isDeviceBinaryFormat<DeviceBinaryFormat::oclElf>(archive)) {
        return unpackSingleDeviceBinary<DeviceBinaryFormat::oclElf>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::patchtokens>(archive)) {
        return unpackSingleDeviceBinary<DeviceBinaryFormat::patchtokens>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::archive>(archive)) {
        return unpackSingleDeviceBinary<DeviceBinaryFormat::archive>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::zebin>(archive)) {
        return unpackSingleDeviceBinary<DeviceBinaryFormat::zebin>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning);
    } else {
        outErrReason = "Unknown format";
    }
    return ret;
}

template <DeviceBinaryFormat format>
std::vector<uint8_t> packDeviceBinary(const SingleDeviceBinary &binary, std::string &outErrReason, std::string &outWarning);

template <>
std::vector<uint8_t> packDeviceBinary<DeviceBinaryFormat::oclElf>(const SingleDeviceBinary &, std::string &, std::string &);

std::vector<uint8_t> packDeviceBinary(const SingleDeviceBinary &binary, std::string &outErrReason, std::string &outWarning);

inline bool isAnyPackedDeviceBinaryFormat(const ArrayRef<const uint8_t> binary) {
    if (isDeviceBinaryFormat<DeviceBinaryFormat::oclElf>(binary)) {
        return true;
    }
    if (isDeviceBinaryFormat<DeviceBinaryFormat::archive>(binary)) {
        return true;
    }
    if (isDeviceBinaryFormat<DeviceBinaryFormat::zebin>(binary)) {
        return true;
    }
    return false;
}

inline bool isAnySingleDeviceBinaryFormat(const ArrayRef<const uint8_t> binary) {
    return ((false == isAnyPackedDeviceBinaryFormat(binary)) && isAnyDeviceBinaryFormat(binary)) || isDeviceBinaryFormat<DeviceBinaryFormat::zebin>(binary);
}

template <DeviceBinaryFormat format>
DecodeError decodeSingleDeviceBinary(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning, const GfxCoreHelper &gfxCoreHelper);

template <>
DecodeError decodeSingleDeviceBinary<DeviceBinaryFormat::oclElf>(ProgramInfo &, const SingleDeviceBinary &, std::string &, std::string &, const GfxCoreHelper &gfxCoreHelper);
template <>
DecodeError decodeSingleDeviceBinary<DeviceBinaryFormat::patchtokens>(ProgramInfo &, const SingleDeviceBinary &, std::string &, std::string &, const GfxCoreHelper &gfxCoreHelper);
template <>
DecodeError decodeSingleDeviceBinary<DeviceBinaryFormat::archive>(ProgramInfo &, const SingleDeviceBinary &, std::string &, std::string &, const GfxCoreHelper &gfxCoreHelper);
template <>
DecodeError decodeSingleDeviceBinary<DeviceBinaryFormat::zebin>(ProgramInfo &, const SingleDeviceBinary &, std::string &, std::string &, const GfxCoreHelper &gfxCoreHelper);

inline std::pair<DecodeError, DeviceBinaryFormat> decodeSingleDeviceBinary(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning, const GfxCoreHelper &gfxCoreHelper) {
    std::pair<DecodeError, DeviceBinaryFormat> ret;
    ret.first = DecodeError::invalidBinary;
    ret.second = DeviceBinaryFormat::unknown;
    if (isDeviceBinaryFormat<DeviceBinaryFormat::oclElf>(src.deviceBinary)) {
        ret.second = DeviceBinaryFormat::oclElf;
        ret.first = decodeSingleDeviceBinary<DeviceBinaryFormat::oclElf>(dst, src, outErrReason, outWarning, gfxCoreHelper);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::patchtokens>(src.deviceBinary)) {
        ret.second = DeviceBinaryFormat::patchtokens;
        ret.first = decodeSingleDeviceBinary<DeviceBinaryFormat::patchtokens>(dst, src, outErrReason, outWarning, gfxCoreHelper);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::archive>(src.deviceBinary)) {
        ret.second = DeviceBinaryFormat::archive;
        ret.first = decodeSingleDeviceBinary<DeviceBinaryFormat::archive>(dst, src, outErrReason, outWarning, gfxCoreHelper);
    } else if (isDeviceBinaryFormat<DeviceBinaryFormat::zebin>(src.deviceBinary)) {
        ret.second = DeviceBinaryFormat::zebin;
        ret.first = decodeSingleDeviceBinary<DeviceBinaryFormat::zebin>(dst, src, outErrReason, outWarning, gfxCoreHelper);
    } else {
        outErrReason = "Unknown format";
    }
    return ret;
}

} // namespace NEO

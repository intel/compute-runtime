/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/ar/ar.h"
#include "core/device_binary_format/ar/ar_decoder.h"
#include "core/device_binary_format/device_binary_formats.h"

namespace NEO {

template <>
bool isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Archive>(const ArrayRef<const uint8_t> binary) {
    return NEO::Ar::isAr(binary);
}

template <>
SingleDeviceBinary unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                                                              std::string &outErrReason, std::string &outWarning) {
    auto archiveData = NEO::Ar::decodeAr(archive, outErrReason, outWarning);
    if (nullptr == archiveData.magic) {
        return {};
    }

    std::string pointerSize = ((requestedTargetDevice.maxPointerSizeInBytes == 8) ? "64" : "32");
    std::string filterPointerSizeAndPlatform = pointerSize + "." + requestedProductAbbreviation.str();
    std::string filterPointerSizeAndPlatformAndStepping = filterPointerSizeAndPlatform + "." + std::to_string(requestedTargetDevice.stepping);

    Ar::ArFileEntryHeaderAndData *matchedFiles[2] = {};
    Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndPlatformAndStepping = matchedFiles[0]; // best match
    Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndPlatform = matchedFiles[1];
    for (auto &f : archiveData.files) {
        if (ConstStringRef(f.fileName.begin(), filterPointerSizeAndPlatform.size()) != filterPointerSizeAndPlatform) {
            continue;
        }

        if (ConstStringRef(f.fileName.begin(), filterPointerSizeAndPlatformAndStepping.size()) != filterPointerSizeAndPlatformAndStepping) {
            matchedPointerSizeAndPlatform = &f;
            continue;
        }
        matchedPointerSizeAndPlatformAndStepping = &f;
    }

    std::string unpackErrors;
    std::string unpackWarnings;
    for (auto matchedFile : matchedFiles) {
        if (nullptr == matchedFile) {
            continue;
        }
        auto unpacked = unpackSingleDeviceBinary(matchedFile->fileData, requestedProductAbbreviation, requestedTargetDevice, unpackErrors, unpackWarnings);
        if (false == unpacked.deviceBinary.empty()) {
            if (matchedFile != matchedPointerSizeAndPlatformAndStepping) {
                outWarning = "Couldn't find perfectly matched binary (right stepping) in AR, using best usable";
            }
            return unpacked;
        }
    }

    outErrReason = "Couldn't find matching binary in AR archive";
    return {};
}

template <>
DecodeError decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning) {
    // packed binary format
    outErrReason = "Device binary format is packed";
    return DecodeError::InvalidBinary;
}

} // namespace NEO

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/ar/ar.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/device_binary_formats.h"

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
    ConstStringRef genericIrFileName{"generic_ir"};

    Ar::ArFileEntryHeaderAndData *matchedFiles[3] = {};
    Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndPlatformAndStepping = matchedFiles[0]; // best match
    Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndPlatform = matchedFiles[1];
    Ar::ArFileEntryHeaderAndData *&matchedGenericIr = matchedFiles[2];

    for (auto &file : archiveData.files) {
        const auto &filename = file.fileName;
        constexpr std::string::size_type zeroIndex{0};

        if (filename.size() >= filterPointerSizeAndPlatformAndStepping.size() &&
            filename.substr(zeroIndex, filterPointerSizeAndPlatformAndStepping.size()) == filterPointerSizeAndPlatformAndStepping) {
            matchedPointerSizeAndPlatformAndStepping = &file;
        } else if (filename.size() >= filterPointerSizeAndPlatform.size() &&
                   filename.substr(zeroIndex, filterPointerSizeAndPlatform.size()) == filterPointerSizeAndPlatform) {
            matchedPointerSizeAndPlatform = &file;
        } else if (file.fileName == genericIrFileName) {
            matchedGenericIr = &file;
        }
    }

    std::string unpackErrors;
    std::string unpackWarnings;
    SingleDeviceBinary binaryForRecompilation = {};
    for (auto matchedFile : matchedFiles) {
        if (nullptr == matchedFile) {
            continue;
        }
        auto unpacked = unpackSingleDeviceBinary(matchedFile->fileData, requestedProductAbbreviation, requestedTargetDevice, unpackErrors, unpackWarnings);
        if (false == unpacked.deviceBinary.empty()) {
            if (matchedFile != matchedPointerSizeAndPlatformAndStepping) {
                outWarning = "Couldn't find perfectly matched binary (right stepping) in AR, using best usable";
            }

            if (unpacked.intermediateRepresentation.empty() && matchedGenericIr) {
                auto unpackedGenericIr = unpackSingleDeviceBinary(matchedGenericIr->fileData, requestedProductAbbreviation, requestedTargetDevice, unpackErrors, unpackWarnings);
                if (!unpackedGenericIr.intermediateRepresentation.empty()) {
                    unpacked.intermediateRepresentation = unpackedGenericIr.intermediateRepresentation;
                }
            }

            return unpacked;
        }
        if (binaryForRecompilation.intermediateRepresentation.empty() && (false == unpacked.intermediateRepresentation.empty())) {
            binaryForRecompilation = unpacked;
        }
    }

    if (false == binaryForRecompilation.intermediateRepresentation.empty()) {
        return binaryForRecompilation;
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

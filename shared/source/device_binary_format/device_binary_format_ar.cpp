/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/ar/ar.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/string.h"

namespace NEO {
void searchForBinary(Ar::Ar &archiveData, const ConstStringRef filter, Ar::ArFileEntryHeaderAndData *&matched) {
    for (auto &file : archiveData.files) {
        if (file.fileName.startsWith(filter.str().c_str())) {
            matched = &file;
            return;
        }
    }
}

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
    std::string filterPointerSizeAndMajorMinorRevision = pointerSize + "." + ProductConfigHelper::parseMajorMinorRevisionValue(requestedTargetDevice.aotConfig);
    std::string filterPointerSizeAndMajorMinor = pointerSize + "." + ProductConfigHelper::parseMajorMinorValue(requestedTargetDevice.aotConfig);
    std::string filterPointerSizeAndPlatform = pointerSize + "." + requestedProductAbbreviation.str();
    std::string filterPointerSizeAndPlatformAndStepping = filterPointerSizeAndPlatform + "." + std::to_string(requestedTargetDevice.stepping);
    ConstStringRef filterGenericIrFileName{"generic_ir"};

    Ar::ArFileEntryHeaderAndData *matchedFiles[5] = {};
    Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndMajorMinorRevision = matchedFiles[0];
    Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndPlatformAndStepping = matchedFiles[1];
    Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndMajorMinor = matchedFiles[2];
    Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndPlatform = matchedFiles[3];
    Ar::ArFileEntryHeaderAndData *&matchedGenericIr = matchedFiles[4];

    searchForBinary(archiveData, ConstStringRef(filterPointerSizeAndMajorMinorRevision), matchedPointerSizeAndMajorMinorRevision);
    searchForBinary(archiveData, ConstStringRef(filterPointerSizeAndPlatformAndStepping), matchedPointerSizeAndPlatformAndStepping);
    searchForBinary(archiveData, ConstStringRef(filterPointerSizeAndMajorMinor), matchedPointerSizeAndMajorMinor);
    searchForBinary(archiveData, ConstStringRef(filterPointerSizeAndPlatform), matchedPointerSizeAndPlatform);
    searchForBinary(archiveData, filterGenericIrFileName, matchedGenericIr);

    std::string unpackErrors;
    std::string unpackWarnings;
    SingleDeviceBinary binaryForRecompilation = {};
    for (auto matchedFile : matchedFiles) {
        if (nullptr == matchedFile) {
            continue;
        }
        auto unpacked = unpackSingleDeviceBinary(matchedFile->fileData, requestedProductAbbreviation, requestedTargetDevice, unpackErrors, unpackWarnings);
        if (false == unpacked.deviceBinary.empty()) {
            if ((matchedFile != matchedPointerSizeAndPlatformAndStepping) && (matchedFile != matchedPointerSizeAndMajorMinorRevision)) {
                outWarning = "Couldn't find perfectly matched binary in AR, using best usable";
            }
            if (unpacked.intermediateRepresentation.empty() && matchedGenericIr) {
                auto unpackedGenericIr = unpackSingleDeviceBinary(matchedGenericIr->fileData, requestedProductAbbreviation, requestedTargetDevice, unpackErrors, unpackWarnings);
                if (!unpackedGenericIr.intermediateRepresentation.empty()) {
                    unpacked.intermediateRepresentation = unpackedGenericIr.intermediateRepresentation;
                }
            }
            unpacked.packedTargetDeviceBinary = ArrayRef<const uint8_t>(matchedFile->fileData.begin(), matchedFile->fileData.size());
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

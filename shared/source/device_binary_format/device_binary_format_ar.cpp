/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/helpers/string.h"

#include "neo_aot_platforms.h"

#include <algorithm>
#include <cstring>
#include <vector>

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
bool isDeviceBinaryFormat<NEO::DeviceBinaryFormat::archive>(const ArrayRef<const uint8_t> binary) {
    return NEO::Ar::isAr(binary);
}

template <>
SingleDeviceBinary unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                                                              std::string &outErrReason, std::string &outWarning) {
    auto archiveData = NEO::Ar::decodeAr(archive, outErrReason, outWarning);
    if (nullptr == archiveData.magic) {
        return {};
    }

    std::string pointerSize = ((requestedTargetDevice.maxPointerSizeInBytes == 8) ? "64" : "32");

    auto searchForProduct = [&](const std::string &productAbbreviation, bool isOriginalProduct) -> SingleDeviceBinary {
        std::string filterPointerSizeAndMajorMinorRevision = pointerSize + "." + ProductConfigHelper::parseMajorMinorRevisionValue(requestedTargetDevice.aotConfig);
        std::string filterPointerSizeAndMajorMinor = pointerSize + "." + ProductConfigHelper::parseMajorMinorValue(requestedTargetDevice.aotConfig);
        std::string filterPointerSizeAndPlatform = pointerSize + "." + productAbbreviation;
        std::string filterPointerSizeAndPlatformAndStepping = filterPointerSizeAndPlatform + "." + std::to_string(requestedTargetDevice.stepping);

        Ar::ArFileEntryHeaderAndData *matchedFiles[4] = {};
        Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndMajorMinorRevision = matchedFiles[0];
        Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndPlatformAndStepping = matchedFiles[1];
        Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndMajorMinor = matchedFiles[2];
        Ar::ArFileEntryHeaderAndData *&matchedPointerSizeAndPlatform = matchedFiles[3];

        searchForBinary(archiveData, ConstStringRef(filterPointerSizeAndMajorMinorRevision), matchedPointerSizeAndMajorMinorRevision);
        searchForBinary(archiveData, ConstStringRef(filterPointerSizeAndPlatformAndStepping), matchedPointerSizeAndPlatformAndStepping);
        searchForBinary(archiveData, ConstStringRef(filterPointerSizeAndMajorMinor), matchedPointerSizeAndMajorMinor);
        searchForBinary(archiveData, ConstStringRef(filterPointerSizeAndPlatform), matchedPointerSizeAndPlatform);

        std::string unpackErrors;
        std::string unpackWarnings;
        SingleDeviceBinary binaryForRecompilation = {};

        for (auto matchedFile : matchedFiles) {
            if (nullptr == matchedFile) {
                continue;
            }
            auto unpacked = unpackSingleDeviceBinary(matchedFile->fileData, ConstStringRef(productAbbreviation), requestedTargetDevice, unpackErrors, unpackWarnings);
            if (false == unpacked.deviceBinary.empty()) {
                if ((matchedFile != matchedPointerSizeAndPlatformAndStepping) && (matchedFile != matchedPointerSizeAndMajorMinorRevision)) {
                    outWarning = "Couldn't find perfectly matched binary in AR, using best usable";
                }
                unpacked.packedTargetDeviceBinary = ArrayRef<const uint8_t>(matchedFile->fileData.begin(), matchedFile->fileData.size());
                return unpacked;
            }
            if (binaryForRecompilation.intermediateRepresentation.empty() && (false == unpacked.intermediateRepresentation.empty())) {
                binaryForRecompilation = unpacked;
            }
        }

        return binaryForRecompilation;
    };

    auto result = searchForProduct(requestedProductAbbreviation.str(), true);
    if (false == result.deviceBinary.empty()) {
        return result;
    }

    SingleDeviceBinary binaryForRecompilation = result;

    auto compatibleProducts = AOT::getCompatibilityFallbackProductAbbreviations(requestedProductAbbreviation.str());
    for (const auto &compatibleProduct : compatibleProducts) {
        auto compatResult = searchForProduct(compatibleProduct, false);
        if (false == compatResult.deviceBinary.empty()) {
            outWarning = "Couldn't find perfectly matched binary in AR, using best usable";
            return compatResult;
        }
        if (binaryForRecompilation.intermediateRepresentation.empty() && (false == compatResult.intermediateRepresentation.empty())) {
            binaryForRecompilation = compatResult;
        }
    }

    ConstStringRef filterGenericIrFileName{"generic_ir"};
    Ar::ArFileEntryHeaderAndData *matchedGenericIr = nullptr;
    searchForBinary(archiveData, filterGenericIrFileName, matchedGenericIr);

    if (matchedGenericIr && binaryForRecompilation.intermediateRepresentation.empty()) {
        binaryForRecompilation.intermediateRepresentation = matchedGenericIr->fileData;
    }

    if (false == binaryForRecompilation.intermediateRepresentation.empty()) {
        return binaryForRecompilation;
    }

    outErrReason = "Couldn't find matching binary in AR archive";
    return {};
}

template <>
DecodeError decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning, const GfxCoreHelper &gfxCoreHelper) {
    // packed binary format
    outErrReason = "Device binary format is packed";
    return DecodeError::invalidBinary;
}

} // namespace NEO

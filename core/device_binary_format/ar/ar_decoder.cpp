/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/ar/ar_decoder.h"

#include <cstdint>

namespace NEO {
namespace Ar {

Ar decodeAr(const ArrayRef<const uint8_t> binary, std::string &outErrReason, std::string &outWarnings) {
    if (false == isAr(binary)) {
        outErrReason = "Not an AR archive - mismatched file signature";
        return {};
    }

    Ar ret;
    ret.magic = reinterpret_cast<const char *>(binary.begin());

    const uint8_t *decodePos = binary.begin() + arMagic.size();
    while (decodePos + sizeof(ArFileEntryHeader) <= binary.end()) {
        auto fileEntryHeader = reinterpret_cast<const ArFileEntryHeader *>(decodePos);
        auto fileEntryDataPos = decodePos + sizeof(ArFileEntryHeader);
        uint64_t fileSize = readDecimal<sizeof(fileEntryHeader->fileSizeInBytes)>(fileEntryHeader->fileSizeInBytes);
        if (fileSize + (fileEntryDataPos - binary.begin()) > binary.size()) {
            outErrReason = "Corrupt AR archive - out of bounds data of file entry with idenfitier '" + std::string(fileEntryHeader->identifier, sizeof(fileEntryHeader->identifier)) + "'";
            return {};
        }

        if (ConstStringRef(fileEntryHeader->trailingMagic) != arFileEntryTrailingMagic) {
            outWarnings.append("File entry header with identifier '" + std::string(fileEntryHeader->identifier, sizeof(fileEntryHeader->identifier)) + "' has invalid header trailing string");
        }

        ArFileEntryHeaderAndData fileEntry = {};
        fileEntry.fileName = readUnpaddedString<sizeof(fileEntryHeader->identifier)>(fileEntryHeader->identifier);
        fileEntry.fullHeader = fileEntryHeader;
        fileEntry.fileData = ArrayRef<const uint8_t>(fileEntryDataPos, static_cast<size_t>(fileSize));

        if (fileEntry.fileName.empty()) {
            if (SpecialFileNames::longFileNamesFile == ConstStringRef(fileEntryHeader->identifier, 2U)) {
                fileEntry.fileName = SpecialFileNames::longFileNamesFile;
                ret.longFileNamesEntry = fileEntry;
            } else {
                outErrReason = "Corrupt AR archive - file entry does not have identifier : '" + std::string(fileEntryHeader->identifier, sizeof(fileEntryHeader->identifier)) + "'";
                return {};
            }
        } else {
            if (SpecialFileNames::longFileNamePrefix == fileEntry.fileName[0]) {
                auto longFileNamePos = readDecimal<sizeof(fileEntryHeader->identifier) - 1>(fileEntryHeader->identifier + 1);
                fileEntry.fileName = readLongFileName(ConstStringRef(reinterpret_cast<const char *>(ret.longFileNamesEntry.fileData.begin()), ret.longFileNamesEntry.fileData.size()), static_cast<size_t>(longFileNamePos));
                if (fileEntry.fileName.empty()) {
                    outErrReason = "Corrupt AR archive - long file name entry has broken identifier : '" + std::string(fileEntryHeader->identifier, sizeof(fileEntryHeader->identifier)) + "'";
                    return {};
                }
            }
            ret.files.push_back(fileEntry);
        }

        decodePos = fileEntryDataPos + fileSize;
        decodePos += fileSize & 1U; // implicit 2-byte alignment
    }
    return ret;
}

} // namespace Ar

} // namespace NEO

/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/ar/ar_encoder.h"

#include "core/helpers/debug_helpers.h"
#include "core/helpers/string.h"

#include <vector>

namespace NEO {
namespace Ar {

ArFileEntryHeader *ArEncoder::appendFileEntry(const ConstStringRef fileName, const ArrayRef<const uint8_t> fileData) {
    if (fileName.size() > sizeof(ArFileEntryHeader::identifier) - 1) {
        return nullptr; // encoding long identifiers is not supported
    }
    if (fileName.empty()) {
        return nullptr;
    }

    auto alignedFileSize = fileData.size() + (fileData.size() & 1U);
    ArFileEntryHeader header = {};

    if (padTo8Bytes && (0 != ((fileEntries.size() + sizeof(ArFileEntryHeader)) % 8))) {
        ArFileEntryHeader paddingHeader = {};
        auto paddingName = "pad_" + std::to_string(paddingEntry++);
        UNRECOVERABLE_IF(paddingName.length() > sizeof(paddingHeader.identifier));
        memcpy_s(paddingHeader.identifier, sizeof(paddingHeader.identifier), paddingName.c_str(), paddingName.size());
        paddingHeader.identifier[paddingName.size()] = SpecialFileNames::fileNameTerminator;
        size_t paddingSize = 8U - ((fileEntries.size() + 2 * sizeof(ArFileEntryHeader)) % 8);
        auto padSizeString = std::to_string(paddingSize);
        memcpy_s(paddingHeader.fileSizeInBytes, sizeof(paddingHeader.fileSizeInBytes), padSizeString.c_str(), padSizeString.size());
        this->fileEntries.reserve(this->fileEntries.size() + sizeof(paddingHeader) + paddingSize + sizeof(header) + alignedFileSize);
        this->fileEntries.insert(this->fileEntries.end(), reinterpret_cast<uint8_t *>(&paddingHeader), reinterpret_cast<uint8_t *>(&paddingHeader + 1));
        this->fileEntries.resize(this->fileEntries.size() + paddingSize, ' ');
    }

    memcpy_s(header.identifier, sizeof(header.identifier), fileName.begin(), fileName.size());
    header.identifier[fileName.size()] = SpecialFileNames::fileNameTerminator;
    auto sizeString = std::to_string(fileData.size());
    UNRECOVERABLE_IF(sizeString.length() > sizeof(header.fileSizeInBytes));
    memcpy_s(header.fileSizeInBytes, sizeof(header.fileSizeInBytes), sizeString.c_str(), sizeString.size());
    this->fileEntries.reserve(this->fileEntries.size() + sizeof(header) + alignedFileSize);
    auto newFileHeaderOffset = this->fileEntries.size();
    this->fileEntries.insert(this->fileEntries.end(), reinterpret_cast<uint8_t *>(&header), reinterpret_cast<uint8_t *>(&header + 1));
    this->fileEntries.insert(this->fileEntries.end(), fileData.begin(), fileData.end());
    this->fileEntries.resize(this->fileEntries.size() + alignedFileSize - fileData.size(), 0U); // implicit 2-byte alignment
    return reinterpret_cast<ArFileEntryHeader *>(this->fileEntries.data() + newFileHeaderOffset);
}

std::vector<uint8_t> ArEncoder::encode() const {
    std::vector<uint8_t> ret;
    ret.insert(ret.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));
    ret.insert(ret.end(), this->fileEntries.begin(), this->fileEntries.end());
    return ret;
}

} // namespace Ar
} // namespace NEO

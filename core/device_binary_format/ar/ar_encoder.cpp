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
    ArFileEntryHeader header = {};
    memcpy_s(header.identifier, sizeof(header.identifier), fileName.begin(), fileName.size());
    header.identifier[fileName.size()] = SpecialFileNames::fileNameTerminator;
    auto sizeString = std::to_string(fileData.size());
    UNRECOVERABLE_IF(sizeString.length() > sizeof(header.fileSizeInBytes));
    memcpy_s(header.fileSizeInBytes, sizeof(header.fileSizeInBytes), sizeString.c_str(), sizeString.size());
    auto alignedFileSize = fileData.size() + (fileData.size() & 1U);
    this->fileEntries.reserve(sizeof(header) + alignedFileSize);
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

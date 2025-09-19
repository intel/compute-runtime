/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_indirect_data.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"

#include <cinttypes>

namespace L0::MCL {

void MutableIndirectData::setAddress(CrossThreadDataOffset offset, uint64_t address, size_t addressSize) {
    if (isDefined(offset)) {
        if (inlineData.begin() != nullptr) {
            if (offset < inlineData.size()) {
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL store address value %" PRIx64 " size %zu in inline at offset %" PRIu16 "\n", address, addressSize, offset);
                memcpy_s(reinterpret_cast<void *>(inlineData.begin() + offset), addressSize, &address, addressSize);
            } else {
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL store address value %" PRIx64 " size %zu in cross-thread minus inline at offset %" PRIu16 "\n", address, addressSize, offset);
                memcpy_s(reinterpret_cast<void *>(crossThreadData.begin() + offset - inlineData.size()), addressSize, &address, addressSize);
            }
        } else {
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL store address value %" PRIx64 " size %zu in cross-thread at offset %" PRIu16 "\n", address, addressSize, offset);
            memcpy_s(reinterpret_cast<void *>(crossThreadData.begin() + offset), addressSize, &address, addressSize);
        }
    }
}

inline void MutableIndirectData::setIfDefined(CrossThreadDataOffset offset, std::array<uint32_t, 3> data) {
    if (isDefined(offset)) {
        // check inline data is present
        if (inlineData.begin() != nullptr) {
            // check offset begins in inline data
            if (offset < inlineData.size()) {
                // check if all data fits in inline data
                if (offset + sizeof(data) <= inlineData.size()) {
                    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL store data in inline at offset %" PRIu16 "\n", offset);
                    memcpy_s(reinterpret_cast<void *>(inlineData.begin() + offset), sizeof(data), data.data(), sizeof(data));
                } else {
                    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL store data in inline split at offset %" PRIu16 "\n", offset);
                    // data is split between inline and crossthread
                    size_t inlineDataCopySize = inlineData.size() - offset;
                    memcpy_s(reinterpret_cast<void *>(inlineData.begin() + offset), inlineDataCopySize, data.data(), inlineDataCopySize);

                    size_t crossThreadDataCopy = sizeof(data) - inlineDataCopySize;
                    auto srcOffsetDataAddress = reinterpret_cast<uintptr_t>(data.data()) + inlineDataCopySize;
                    memcpy_s(reinterpret_cast<void *>(crossThreadData.begin()), crossThreadDataCopy, reinterpret_cast<void *>(srcOffsetDataAddress), crossThreadDataCopy);
                }
            } else {
                // offset does not start in existing inline, decrease crossthread offset by inline data size
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL store data in cross-thread minus inline at offset %" PRIu16 "\n", offset);
                memcpy_s(reinterpret_cast<void *>(crossThreadData.begin() + offset - inlineData.size()), sizeof(data), data.data(), sizeof(data));
            }
        } else {
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL store data in cross-thread at offset %" PRIu16 "\n", offset);
            memcpy_s(reinterpret_cast<void *>(crossThreadData.begin() + offset), sizeof(data), data.data(), sizeof(data));
        }
    }
}

void MutableIndirectData::setLocalWorkSize(std::array<uint32_t, 3> localWorkSize) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutation set lws %u %u %u\n", localWorkSize[0], localWorkSize[1], localWorkSize[2]);
    setIfDefined(offsets->localWorkSize, localWorkSize);
}

void MutableIndirectData::setLocalWorkSize2(std::array<uint32_t, 3> localWorkSize2) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutation set lws2 %u %u %u\n", localWorkSize2[0], localWorkSize2[1], localWorkSize2[2]);
    setIfDefined(offsets->localWorkSize2, localWorkSize2);
}

void MutableIndirectData::setEnqLocalWorkSize(std::array<uint32_t, 3> enqLocalWorkSize) {
    setIfDefined(offsets->enqLocalWorkSize, enqLocalWorkSize);
}

void MutableIndirectData::setGlobalWorkSize(std::array<uint32_t, 3> globalWorkSize) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutation set gws %u %u %u\n", globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);
    setIfDefined(offsets->globalWorkSize, globalWorkSize);
}

void MutableIndirectData::setNumWorkGroups(std::array<uint32_t, 3> numWorkGroups) {
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL mutation set num wgs %u %u %u\n", numWorkGroups[0], numWorkGroups[1], numWorkGroups[2]);
    setIfDefined(offsets->numWorkGroups, numWorkGroups);
}

void MutableIndirectData::setWorkDimensions(uint32_t workDimensions) {
    if (isDefined(offsets->workDimensions)) {
        if (inlineData.begin() != nullptr) {
            if (offsets->workDimensions < inlineData.size()) {
                *reinterpret_cast<uint32_t *>(inlineData.begin() + offsets->workDimensions) = workDimensions;
            } else {
                *reinterpret_cast<uint32_t *>(crossThreadData.begin() + offsets->workDimensions - inlineData.size()) = workDimensions;
            }
        } else {
            *reinterpret_cast<uint32_t *>(crossThreadData.begin() + offsets->workDimensions) = workDimensions;
        }
    }
}

void MutableIndirectData::setGlobalWorkOffset(std::array<uint32_t, 3> globalWorkOffset) {
    setIfDefined(offsets->globalWorkOffset, globalWorkOffset);
}

void MutableIndirectData::setPerThreadData(ArrayRef<const uint8_t> perThreadData) {
    UNRECOVERABLE_IF(this->perThreadData.size() < perThreadData.size());
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintMclData.get(), stderr, "MCL copy local IDs into per-thread %p\n", this->perThreadData.begin());
    memcpy_s(this->perThreadData.begin(), this->perThreadData.size(),
             perThreadData.begin(), perThreadData.size());
}

void MutableIndirectData::setCrossThreadData(ArrayRef<const uint8_t> crossThreadData) {
    UNRECOVERABLE_IF(this->crossThreadData.size() < crossThreadData.size());
    memcpy_s(this->crossThreadData.begin(), this->crossThreadData.size(),
             crossThreadData.begin(), crossThreadData.size());
}

}; // namespace L0::MCL

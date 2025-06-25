/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_indirect_data_fixture.h"

#include "shared/source/helpers/aligned_memory.h"

namespace L0 {
namespace ult {

void MutableIndirectDataFixture::createMutableIndirectOffset() {
    auto indirectOffsets = std::make_unique<L0::MCL::MutableIndirectData::Offsets>();
    indirectOffsets->globalWorkSize = offsets.globalWorkSize;
    indirectOffsets->localWorkSize = offsets.localWorkSize;
    indirectOffsets->localWorkSize2 = offsets.localWorkSize2;
    indirectOffsets->enqLocalWorkSize = offsets.enqLocalWorkSize;
    indirectOffsets->numWorkGroups = offsets.numWorkGroups;
    indirectOffsets->workDimensions = offsets.workDimensions;
    indirectOffsets->globalWorkOffset = offsets.globalWorkOffset;

    ArrayRef<uint8_t> crossThreadDataRef;
    ArrayRef<uint8_t> perThreadDataRef;
    ArrayRef<uint8_t> inlineDataRef;

    this->crossThreadData = std::unique_ptr<uint8_t, decltype(&alignedFree)>(reinterpret_cast<uint8_t *>(alignedMalloc(this->crossThreadDataSize, 32)), &alignedFree);
    memset(this->crossThreadData.get(), 0, this->crossThreadDataSize);
    crossThreadDataRef = {this->crossThreadData.get(), this->crossThreadDataSize};

    if (this->perThreadDataSize) {
        this->perThreadData = std::unique_ptr<uint8_t, decltype(&alignedFree)>(reinterpret_cast<uint8_t *>(alignedMalloc(this->perThreadDataSize, 32)), &alignedFree);
        memset(this->perThreadData.get(), 0, this->perThreadDataSize);
        perThreadDataRef = {this->perThreadData.get(), this->perThreadDataSize};
    }
    if (this->inlineSize) {
        this->inlineData = std::unique_ptr<uint8_t, decltype(&alignedFree)>(reinterpret_cast<uint8_t *>(alignedMalloc(this->inlineSize, 32)), &alignedFree);
        memset(this->inlineData.get(), 0, this->inlineSize);
        inlineDataRef = {this->inlineData.get(), this->inlineSize};
    }

    this->indirectData = std::make_unique<L0::MCL::MutableIndirectData>(std::move(indirectOffsets), crossThreadDataRef, perThreadDataRef, inlineDataRef);
    this->mockMutableIndirectData = static_cast<MockMutableIndirectData *>(this->indirectData.get());
}

void MutableIndirectDataFixture::recalculateOffsets(L0::MCL::CrossThreadDataOffset initialOffset) {
    offsets.globalWorkSize = initialOffset;
    offsets.localWorkSize = initialOffset + Offsets::patchSize;
    offsets.localWorkSize2 = initialOffset + 2 * Offsets::patchSize;
    offsets.enqLocalWorkSize = initialOffset + 3 * Offsets::patchSize;
    offsets.numWorkGroups = initialOffset + 4 * Offsets::patchSize;
    offsets.workDimensions = initialOffset + 5 * Offsets::patchSize;
    offsets.globalWorkOffset = initialOffset + 6 * Offsets::patchSize;
}

void MutableIndirectDataFixture::undefineOffsets() {
    offsets.globalWorkSize = undefinedOffset;
    offsets.localWorkSize = undefinedOffset;
    offsets.localWorkSize2 = undefinedOffset;
    offsets.enqLocalWorkSize = undefinedOffset;
    offsets.numWorkGroups = undefinedOffset;
    offsets.workDimensions = undefinedOffset;
    offsets.globalWorkOffset = undefinedOffset;
}

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace L0 {
namespace Sysman {
namespace ult {

class MockCperRecord {
  public:
    using Guid = std::array<uint8_t, 16>;

    static constexpr uint32_t recordHeaderSize = 128u;
    static constexpr uint32_t sectionCountOffset = 10u;
    static constexpr uint32_t recordLengthOffset = 20u;
    static constexpr uint32_t sectionDescriptorSize = 72u;
    static constexpr uint32_t sectionOffsetOffset = 0u;
    static constexpr uint32_t sectionLengthOffset = 4u;
    static constexpr uint32_t sectionValidationBitsOffset = 10u;
    static constexpr uint32_t sectionTypeOffset = 16u;
    static constexpr uint32_t sectionFruIdOffset = 32u;
    static constexpr uint8_t fruIdValidBit = 0x1u;
    static constexpr uint32_t headerValidationBitsOffset = 28u;
    static constexpr uint16_t sigIdValidBit = 0x4u;

    // Deliberately an independent copy of the section type the decoder knows, so that a wrong value
    // in the decoder fails these tests instead of matching itself.
    static constexpr Guid acceleratorGuid = {0x84, 0x8f, 0x9d, 0xea, 0x58, 0x42, 0x27, 0x52, 0x80, 0x28, 0xb9, 0xb1, 0x3e, 0x6d, 0x58, 0xb0};
    static constexpr Guid unknownGuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};

    static constexpr Guid sampleFruId = {0x05, 0x1c, 0x3f, 0x6a, 0x2b, 0x1a, 0x4d, 0x3c, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

    void addSection(const Guid &guid, uint32_t bodyLength, uint32_t sigIdOffset, uint32_t sigId) {
        sections.push_back({guid, Guid{}, bodyLength, sigIdOffset, sigId, false, true});
    }

    void addSectionWithFruId(const Guid &guid, uint32_t bodyLength, uint32_t sigIdOffset, uint32_t sigId, const Guid &fruId) {
        sections.push_back({guid, fruId, bodyLength, sigIdOffset, sigId, true, true});
    }

    void addSectionWithInvalidSigId(const Guid &guid, uint32_t bodyLength, uint32_t sigIdOffset, uint32_t sigId) {
        sections.push_back({guid, Guid{}, bodyLength, sigIdOffset, sigId, false, false});
    }

    void addSectionWithFruIdAndInvalidSigId(const Guid &guid, uint32_t bodyLength, uint32_t sigIdOffset, uint32_t sigId, const Guid &fruId) {
        sections.push_back({guid, fruId, bodyLength, sigIdOffset, sigId, true, false});
    }

    std::vector<uint8_t> build() const {
        uint32_t length = recordHeaderSize + static_cast<uint32_t>(sections.size()) * sectionDescriptorSize;
        for (const auto &section : sections) {
            length += section.bodyLength;
        }

        std::vector<uint8_t> record(length, 0);
        std::memcpy(record.data(), "CPER", 4);
        writeU16(record, sectionCountOffset, static_cast<uint16_t>(sections.size()));
        writeU32(record, recordLengthOffset, length);

        uint32_t bodyOffset = recordHeaderSize + static_cast<uint32_t>(sections.size()) * sectionDescriptorSize;
        for (size_t i = 0; i < sections.size(); i++) {
            const auto &section = sections[i];
            uint32_t descriptor = recordHeaderSize + static_cast<uint32_t>(i) * sectionDescriptorSize;
            writeU32(record, descriptor + sectionOffsetOffset, bodyOffset);
            writeU32(record, descriptor + sectionLengthOffset, section.bodyLength);
            std::memcpy(record.data() + descriptor + sectionTypeOffset, section.guid.data(), section.guid.size());
            if (section.withFruId) {
                record[descriptor + sectionValidationBitsOffset] |= fruIdValidBit;
                std::memcpy(record.data() + descriptor + sectionFruIdOffset, section.fruId.data(), section.fruId.size());
            }
            if (section.sigIdOffset + sizeof(uint32_t) <= section.bodyLength) {
                writeU32(record, bodyOffset + section.sigIdOffset, section.sigId);
            }
            if (section.sigIdValid && headerValidationBitsOffset + sizeof(uint16_t) <= section.bodyLength) {
                writeU16(record, bodyOffset + headerValidationBitsOffset, sigIdValidBit);
            }
            bodyOffset += section.bodyLength;
        }
        return record;
    }

    static std::vector<uint8_t> sampleRecord() {
        MockCperRecord builder;
        builder.addSectionWithFruId(acceleratorGuid, 332, 20, 9, sampleFruId);
        return builder.build();
    }

    static void writeU16(std::vector<uint8_t> &record, uint32_t offset, uint16_t value) {
        std::memcpy(record.data() + offset, &value, sizeof(value));
    }

    static void writeU32(std::vector<uint8_t> &record, uint32_t offset, uint32_t value) {
        std::memcpy(record.data() + offset, &value, sizeof(value));
    }

  private:
    struct Section {
        Guid guid;
        Guid fruId;
        uint32_t bodyLength;
        uint32_t sigIdOffset;
        uint32_t sigId;
        bool withFruId;
        bool sigIdValid;
    };
    std::vector<Section> sections;
};

} // namespace ult
} // namespace Sysman
} // namespace L0

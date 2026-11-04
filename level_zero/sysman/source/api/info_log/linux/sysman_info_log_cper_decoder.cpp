/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/linux/sysman_info_log_cper_decoder.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/preprocessor.h"
#include "shared/source/helpers/string.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace L0 {
namespace Sysman {

constexpr uint32_t recordHeaderSize = 128u;
constexpr uint32_t sectionCountOffset = 10u;
constexpr uint32_t recordLengthOffset = 20u;
constexpr uint32_t sectionDescriptorSize = 72u;
constexpr uint32_t sectionOffsetOffset = 0u;
constexpr uint32_t sectionLengthOffset = 4u;
constexpr uint32_t sectionValidationBitsOffset = 10u;
constexpr uint32_t sectionTypeOffset = 16u;
constexpr uint32_t sectionFruIdOffset = 32u;
constexpr uint8_t fruIdValidBit = 0x1u;
constexpr uint8_t recordSignature[] = {'C', 'P', 'E', 'R'};

// Offsets into the body of a section whose type is the shared accelerator section type, which the
// Gen1 error header definition lays out as location(12) first_timestamp(8) sig_id(4) error_count(4)
// validation_bits(2), with bit 2 of the validation bits reporting whether sig_id was populated.
constexpr uint32_t errorHeaderSigIdOffset = 20u;
constexpr uint32_t errorHeaderValidationBitsOffset = 28u;
constexpr uint16_t errorHeaderSigIdValidBit = 0x4u;

struct FieldDescription {
    const char *desc;
    const char *name;
    zes_intel_info_log_field_type_exp_t type;
    size_t requiredBytes;
    size_t elementCount;
    const char *versionString;
};

struct SectionTypeDescription {
    std::array<uint8_t, ZES_MAX_UUID_SIZE> guid;
    uint32_t sigIdOffset;
    uint32_t validationBitsOffset;
    uint16_t sigIdValidBit;
};

struct KnownSection {
    const uint8_t *pDescriptor;
    const uint8_t *pBody;
    uint32_t bodyLength;
    const SectionTypeDescription *pType;
};

using FieldReader = ze_result_t (*)(const KnownSection &section, void *pOutBuffer, size_t *pBytesCopied);

constexpr std::array<FieldDescription, CperDecoder::fieldCount> fields = {{
    {"Signature ID of the event the record describes.",
     "sig_id", ZES_INTEL_INFO_LOG_FIELD_TYPE_EXP_UINT32, sizeof(uint32_t), 1u, "1.0"},
    {"UUID reported by the record.",
     "uuid", ZES_INTEL_INFO_LOG_FIELD_TYPE_EXP_RAW_BYTES, ZES_MAX_UUID_SIZE, ZES_MAX_UUID_SIZE, "1.0"},
}};

// The accelerator section type, in the byte order a section descriptor stores it in. It is shared by
// every producer of these records, so it is the only section type a sig_id can be decoded from.
constexpr std::array<SectionTypeDescription, 1> sectionTypes = {{
    {{0x84, 0x8f, 0x9d, 0xea, 0x58, 0x42, 0x27, 0x52, 0x80, 0x28, 0xb9, 0xb1, 0x3e, 0x6d, 0x58, 0xb0}, errorHeaderSigIdOffset, errorHeaderValidationBitsOffset, errorHeaderSigIdValidBit},
}};

template <typename T>
static T readValue(const uint8_t *pData) {
    T value;
    std::memcpy(&value, pData, sizeof(T));
    return value;
}

static const SectionTypeDescription *findSectionType(const uint8_t *pGuid) {
    auto it = std::find_if(sectionTypes.begin(), sectionTypes.end(), [pGuid](const SectionTypeDescription &sectionType) {
        return std::memcmp(sectionType.guid.data(), pGuid, ZES_MAX_UUID_SIZE) == 0;
    });
    return it == sectionTypes.end() ? nullptr : &*it;
}

static bool isSigIdValid(const KnownSection &section) {
    if (static_cast<uint64_t>(section.pType->validationBitsOffset) + sizeof(uint16_t) > section.bodyLength) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Section of %u bytes is too short to hold the validation bits at offset %u\n", NEO_FUNCTION_NAME, section.bodyLength, section.pType->validationBitsOffset);
        return false;
    }
    return (readValue<uint16_t>(section.pBody + section.pType->validationBitsOffset) & section.pType->sigIdValidBit) != 0u;
}

static bool isRecordWellFormed(uint32_t size, const uint8_t *pRecord, uint32_t &sectionCount) {
    if (size < recordHeaderSize) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Record size %u is smaller than a CPER record header\n", NEO_FUNCTION_NAME, size);
        return false;
    }
    if (std::memcmp(pRecord, recordSignature, sizeof(recordSignature)) != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Record does not start with the CPER signature\n", NEO_FUNCTION_NAME);
        return false;
    }

    auto recordLength = readValue<uint32_t>(pRecord + recordLengthOffset);
    if (recordLength < recordHeaderSize || recordLength > size) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Record length %u is not within [%u, %u]\n", NEO_FUNCTION_NAME, recordLength, recordHeaderSize, size);
        return false;
    }

    sectionCount = readValue<uint16_t>(pRecord + sectionCountOffset);
    if (recordHeaderSize + static_cast<uint64_t>(sectionCount) * sectionDescriptorSize > recordLength) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): %u section descriptors do not fit in a record of %u bytes\n", NEO_FUNCTION_NAME, sectionCount, recordLength);
        return false;
    }

    for (uint32_t i = 0; i < sectionCount; i++) {
        const uint8_t *pDescriptor = pRecord + recordHeaderSize + i * sectionDescriptorSize;
        auto sectionOffset = readValue<uint32_t>(pDescriptor + sectionOffsetOffset);
        auto sectionLength = readValue<uint32_t>(pDescriptor + sectionLengthOffset);
        if (static_cast<uint64_t>(sectionOffset) + sectionLength > recordLength) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Section %u at offset %u with length %u lies outside a record of %u bytes\n", NEO_FUNCTION_NAME, i, sectionOffset, sectionLength, recordLength);
            return false;
        }
    }

    return true;
}

static bool findFirstKnownSection(uint32_t recordSize, const uint8_t *pRecord, KnownSection &section) {
    uint32_t sectionCount = 0;
    if (!isRecordWellFormed(recordSize, pRecord, sectionCount)) {
        return false;
    }

    for (uint32_t i = 0; i < sectionCount; i++) {
        const uint8_t *pDescriptor = pRecord + recordHeaderSize + i * sectionDescriptorSize;
        auto pSectionType = findSectionType(pDescriptor + sectionTypeOffset);
        if (pSectionType == nullptr) {
            continue;
        }

        section.pDescriptor = pDescriptor;
        section.pBody = pRecord + readValue<uint32_t>(pDescriptor + sectionOffsetOffset);
        section.bodyLength = readValue<uint32_t>(pDescriptor + sectionLengthOffset);
        section.pType = pSectionType;
        return true;
    }

    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): None of the %u sections of the record has a decodable section type\n", NEO_FUNCTION_NAME, sectionCount);
    return false;
}

static ze_result_t readSigId(const KnownSection &section, void *pOutBuffer, size_t *pBytesCopied) {
    if (static_cast<uint64_t>(section.pType->sigIdOffset) + sizeof(uint32_t) > section.bodyLength) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Section of %u bytes is too short to hold a sig_id at offset %u, returning error: 0x%x\n", NEO_FUNCTION_NAME, section.bodyLength, section.pType->sigIdOffset, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    if (!isSigIdValid(section)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): The section does not carry a valid sig_id, returning error: 0x%x\n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto sigId = readValue<uint32_t>(section.pBody + section.pType->sigIdOffset);
    std::memcpy(pOutBuffer, &sigId, sizeof(sigId));
    *pBytesCopied = sizeof(sigId);
    return ZE_RESULT_SUCCESS;
}

static ze_result_t readUuid(const KnownSection &section, void *pOutBuffer, size_t *pBytesCopied) {
    if ((readValue<uint8_t>(section.pDescriptor + sectionValidationBitsOffset) & fruIdValidBit) == 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): The section descriptor does not mark its FRU id valid, returning error: 0x%x\n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    std::memcpy(pOutBuffer, section.pDescriptor + sectionFruIdOffset, ZES_MAX_UUID_SIZE);
    *pBytesCopied = ZES_MAX_UUID_SIZE;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CperDecoder::getFieldInfo(uint32_t *pCount, zes_intel_info_log_decodable_field_info_exp_t *pFieldInfos) {
    if (*pCount == 0) {
        *pCount = fieldCount;
        return ZE_RESULT_SUCCESS;
    }
    auto infoCount = std::min(*pCount, static_cast<uint32_t>(fieldCount));

    if (nullptr != pFieldInfos) {
        for (uint32_t i = 0; i < infoCount; i++) {
            if (pFieldInfos[i].pNext != nullptr) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): pFieldInfos[%u].pNext is not nullptr, extension structures are not supported, returning error: 0x%x\n", NEO_FUNCTION_NAME, i, ZE_RESULT_ERROR_INVALID_ARGUMENT);
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }

        for (uint32_t i = 0; i < infoCount; i++) {
            auto &info = pFieldInfos[i];
            strncpy_s(info.desc, sizeof(info.desc), fields[i].desc, sizeof(info.desc) - 1);
            strncpy_s(info.name, sizeof(info.name), fields[i].name, sizeof(info.name) - 1);
            info.fieldId = i;
            info.type = fields[i].type;
            info.requiredBytes = fields[i].requiredBytes;
            info.elementCount = fields[i].elementCount;
            strncpy_s(info.versionString, sizeof(info.versionString), fields[i].versionString, sizeof(info.versionString) - 1);
        }
    }

    *pCount = infoCount;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CperDecoder::validateRecords(uint32_t size, uint32_t recordCount, const zes_info_log_metadata_ext_t *pDescriptors) {
    for (uint32_t r = 0; r < recordCount; r++) {
        if (static_cast<uint64_t>(pDescriptors[r].offset) + pDescriptors[r].lengthOfData > size) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Record %u at offset %u with length %u lies outside a buffer of %u bytes, returning error: 0x%x\n", NEO_FUNCTION_NAME, r, pDescriptors[r].offset, pDescriptors[r].lengthOfData, size, ZE_RESULT_ERROR_INVALID_ARGUMENT);
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t CperDecoder::readField(uint32_t size, const uint8_t *pBuffer, const zes_info_log_metadata_ext_t *pDescriptors,
                                   uint32_t fieldId, uint32_t recordId,
                                   void *pOutBuffer, size_t bufferLen, size_t *pBytesCopied) {
    *pBytesCopied = 0;

    FieldReader reader = nullptr;
    switch (fieldId) {
    case sigIdField:
        reader = readSigId;
        break;
    case uuidField:
        reader = readUuid;
        break;
    default:
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Field %u is not decodable, returning error: 0x%x\n", NEO_FUNCTION_NAME, fieldId, ZE_RESULT_ERROR_INVALID_ARGUMENT);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    const auto &field = fields[fieldId];
    if (bufferLen < field.requiredBytes) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Buffer of %zu bytes cannot hold field '%s' of %zu bytes, returning error: 0x%x\n", NEO_FUNCTION_NAME, bufferLen, field.name, field.requiredBytes, ZE_RESULT_ERROR_INVALID_SIZE);
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    if (validateRecords(size, 1u, pDescriptors + recordId) != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Record %u does not lie within the buffer being decoded, returning error: 0x%x\n", NEO_FUNCTION_NAME, recordId, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    KnownSection section = {};
    if (!findFirstKnownSection(pDescriptors[recordId].lengthOfData, pBuffer + pDescriptors[recordId].offset, section)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Record %u carries no section the field '%s' could be read from, returning error: 0x%x\n", NEO_FUNCTION_NAME, recordId, field.name, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return reader(section, pOutBuffer, pBytesCopied);
}

} // namespace Sysman
} // namespace L0

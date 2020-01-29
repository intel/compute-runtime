/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/compiler_interface/compiler_options/compiler_options_base.h"
#include "core/utilities/const_stringref.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace NEO {

namespace KernelArgMetadata {

enum class AccessQualifier : uint8_t {
    Unknown,
    None,
    ReadOnly,
    WriteOnly,
    ReadWrite,
};

namespace AccessQualifierStrings {
constexpr ConstStringRef none = "NONE";
constexpr ConstStringRef readOnly = "read_only";
constexpr ConstStringRef writeOnly = "write_only";
constexpr ConstStringRef readWrite = "read_write";
constexpr ConstStringRef underscoreReadOnly = "__read_only";
constexpr ConstStringRef underscoreWriteOnly = "__write_only";
constexpr ConstStringRef underscoreReadWrite = "__read_write";
} // namespace AccessQualifierStrings

enum class AddressSpaceQualifier : uint8_t {
    Unknown,
    Global,
    Local,
    Private,
    Constant
};

namespace AddressSpaceQualifierStrings {
constexpr ConstStringRef addrGlobal = "__global";
constexpr ConstStringRef addrLocal = "__local";
constexpr ConstStringRef addrPrivate = "__private";
constexpr ConstStringRef addrConstant = "__constant";
constexpr ConstStringRef addrNotSpecified = "not_specified";
} // namespace AddressSpaceQualifierStrings

constexpr AccessQualifier parseAccessQualifier(ConstStringRef str) {
    using namespace AccessQualifierStrings;
    if (str.empty() || (none == str)) {
        return AccessQualifier::None;
    }

    if (str.length() < 3) {
        return AccessQualifier::Unknown;
    }

    ConstStringRef strNoUnderscore = ('_' == str[0]) ? ConstStringRef(str.data() + 2, str.length() - 2) : str;
    static_assert(writeOnly[0] != readOnly[0], "");
    static_assert(writeOnly[0] != readWrite[0], "");
    if (strNoUnderscore[0] == writeOnly[0]) {
        return (writeOnly == strNoUnderscore) ? AccessQualifier::WriteOnly : AccessQualifier::Unknown;
    }

    if (readOnly == strNoUnderscore) {
        return AccessQualifier::ReadOnly;
    }

    return (readWrite == strNoUnderscore) ? AccessQualifier::ReadWrite : AccessQualifier::Unknown;
}

constexpr AddressSpaceQualifier parseAddressSpace(ConstStringRef str) {
    using namespace AddressSpaceQualifierStrings;
    if (str.empty()) {
        return AddressSpaceQualifier::Global;
    }

    if (str.length() < 3) {
        return AddressSpaceQualifier::Unknown;
    }

    switch (str[2]) {
    default:
        return AddressSpaceQualifier::Unknown;
    case addrNotSpecified[2]:
        return (str == addrNotSpecified) ? AddressSpaceQualifier::Private : AddressSpaceQualifier::Unknown;
    case addrGlobal[2]:
        return (str == addrGlobal) ? AddressSpaceQualifier::Global : AddressSpaceQualifier::Unknown;
    case addrLocal[2]:
        return (str == addrLocal) ? AddressSpaceQualifier::Local : AddressSpaceQualifier::Unknown;
    case addrPrivate[2]:
        return (str == addrPrivate) ? AddressSpaceQualifier::Private : AddressSpaceQualifier::Unknown;
    case addrConstant[2]:
        return (str == addrConstant) ? AddressSpaceQualifier::Constant : AddressSpaceQualifier::Unknown;
    }
}

union TypeQualifiers {
    uint8_t packed = 0U;
    struct {
        bool constQual : 1;
        bool volatileQual : 1;
        bool restrictQual : 1;
        bool pipeQual : 1;
        bool unknownQual : 1;
    };
    bool empty() const {
        return 0U == packed;
    }
};

namespace TypeQualifierStrings {
constexpr ConstStringRef qualConst = "const";
constexpr ConstStringRef qualVolatile = "volatile";
constexpr ConstStringRef qualRestrict = "restrict";
constexpr ConstStringRef qualPipe = "pipe";
} // namespace TypeQualifierStrings

inline TypeQualifiers parseTypeQualifiers(ConstStringRef str) {
    using namespace TypeQualifierStrings;
    TypeQualifiers ret = {};
    auto tokenized = CompilerOptions::tokenize(str);
    for (const auto &tok : tokenized) {
        bool knownQualifier = true;
        switch (tok[0]) {
        default:
            knownQualifier = false;
            break;
        case qualConst[0]:
            knownQualifier = (qualConst == tok);
            ret.constQual |= knownQualifier;
            break;
        case qualVolatile[0]:
            knownQualifier = (qualVolatile == tok);
            ret.volatileQual |= knownQualifier;
            break;
        case qualRestrict[0]:
            knownQualifier = (qualRestrict == tok);
            ret.restrictQual |= knownQualifier;
            break;
        case qualPipe[0]:
            knownQualifier = (qualPipe == tok);
            ret.pipeQual |= knownQualifier;
            break;
        }
        ret.unknownQual |= !knownQualifier;
    }
    return ret;
}

} // namespace KernelArgMetadata

inline std::string parseLimitedString(const char *str, size_t maxSize) {
    std::string ret{str, str + maxSize};
    size_t minSize = strlen(ret.c_str());
    ret.assign(str, minSize);
    return ret;
}

struct ArgTypeMetadata {
    uint32_t argByValSize = 0U;
    KernelArgMetadata::AccessQualifier accessQualifier = {};
    KernelArgMetadata::AddressSpaceQualifier addressQualifier = {};
    KernelArgMetadata::TypeQualifiers typeQualifiers = {};
};
static_assert(sizeof(ArgTypeMetadata) <= 8, "");

struct ArgTypeMetadataExtended {
    std::string argName;
    std::string type;
    std::string accessQualifier;
    std::string addressQualifier;
    std::string typeQualifiers;
};

struct KernelArgPatchInfo {
    uint32_t crossthreadOffset = 0;
    uint32_t size = 0;
    uint32_t sourceOffset = 0;
};

struct KernelArgInfo {
    KernelArgInfo() {
        this->metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    }
    ~KernelArgInfo() = default;
    KernelArgInfo(const KernelArgInfo &rhs) = delete;
    KernelArgInfo &operator=(const KernelArgInfo &) = delete;
    KernelArgInfo(KernelArgInfo &&) = default;
    KernelArgInfo &operator=(KernelArgInfo &&) = default;

    static constexpr uint32_t undefinedOffset = (uint32_t)-1;

    ArgTypeMetadata metadata;
    std::unique_ptr<ArgTypeMetadataExtended> metadataExtended;

    uint32_t slmAlignment = 0;
    bool isImage = false;
    bool isMediaImage = false;
    bool isMediaBlockImage = false;
    bool isSampler = false;
    bool isAccelerator = false;
    bool isDeviceQueue = false;
    bool isBuffer = false;
    bool pureStatefulBufferAccess = false;
    bool isReadOnly = false;
    bool needPatch = false;
    bool isTransformable = false;

    uint32_t offsetHeap = 0;
    std::vector<KernelArgPatchInfo> kernelArgPatchInfoVector;
    uint32_t samplerArgumentType = 0;
    uint32_t offsetImgWidth = undefinedOffset;
    uint32_t offsetImgHeight = undefinedOffset;
    uint32_t offsetImgDepth = undefinedOffset;
    uint32_t offsetChannelDataType = undefinedOffset;
    uint32_t offsetChannelOrder = undefinedOffset;
    uint32_t offsetArraySize = undefinedOffset;
    uint32_t offsetNumSamples = undefinedOffset;
    uint32_t offsetSamplerSnapWa = undefinedOffset;
    uint32_t offsetSamplerAddressingMode = undefinedOffset;
    uint32_t offsetSamplerNormalizedCoords = undefinedOffset;
    uint32_t offsetVmeMbBlockType = undefinedOffset;
    uint32_t offsetVmeSubpixelMode = undefinedOffset;
    uint32_t offsetVmeSadAdjustMode = undefinedOffset;
    uint32_t offsetVmeSearchPathType = undefinedOffset;
    uint32_t offsetObjectId = undefinedOffset;
    uint32_t offsetBufferOffset = undefinedOffset;
    uint32_t offsetNumMipLevels = undefinedOffset;
    uint32_t offsetFlatBaseOffset = undefinedOffset;
    uint32_t offsetFlatWidth = undefinedOffset;
    uint32_t offsetFlatHeight = undefinedOffset;
    uint32_t offsetFlatPitch = undefinedOffset;
};

} // namespace NEO

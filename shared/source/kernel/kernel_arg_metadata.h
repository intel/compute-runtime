/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_options/compiler_options_base.h"
#include "shared/source/utilities/const_stringref.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

namespace NEO {

namespace KernelArgMetadata {

enum AccessQualifier : uint8_t {
    AccessUnknown,
    AccessNone,
    AccessReadOnly,
    AccessWriteOnly,
    AccessReadWrite,
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

enum AddressSpaceQualifier : uint8_t {
    AddrUnknown,
    AddrGlobal,
    AddrLocal,
    AddrPrivate,
    AddrConstant
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
        return AccessNone;
    }

    if (str.length() < 3) {
        return AccessUnknown;
    }

    ConstStringRef strNoUnderscore = ('_' == str[0]) ? ConstStringRef(str.data() + 2, str.length() - 2) : str;
    static_assert(writeOnly[0] != readOnly[0], "");
    static_assert(writeOnly[0] != readWrite[0], "");
    if (strNoUnderscore[0] == writeOnly[0]) {
        return (writeOnly == strNoUnderscore) ? AccessWriteOnly : AccessUnknown;
    }

    if (readOnly == strNoUnderscore) {
        return AccessReadOnly;
    }

    return (readWrite == strNoUnderscore) ? AccessReadWrite : AccessUnknown;
}

constexpr AddressSpaceQualifier parseAddressSpace(ConstStringRef str) {
    using namespace AddressSpaceQualifierStrings;
    if (str.empty()) {
        return AddrGlobal;
    }

    if (str.length() < 3) {
        return AddrUnknown;
    }

    switch (str[2]) {
    default:
        return AddrUnknown;
    case addrNotSpecified[2]:
        return (str == addrNotSpecified) ? AddrPrivate : AddrUnknown;
    case addrGlobal[2]:
        return (str == addrGlobal) ? AddrGlobal : AddrUnknown;
    case addrLocal[2]:
        return (str == addrLocal) ? AddrLocal : AddrUnknown;
    case addrPrivate[2]:
        return (str == addrPrivate) ? AddrPrivate : AddrUnknown;
    case addrConstant[2]:
        return (str == addrConstant) ? AddrConstant : AddrUnknown;
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

struct ArgTypeTraits {
    ArgTypeTraits() {
        accessQualifier = KernelArgMetadata::AccessUnknown;
        addressQualifier = KernelArgMetadata::AddrGlobal;
    }
    uint16_t argByValSize = 0U;
    struct {
        std::underlying_type_t<KernelArgMetadata::AccessQualifier> accessQualifier : 4;
        std::underlying_type_t<KernelArgMetadata::AddressSpaceQualifier> addressQualifier : 4;
    };
    KernelArgMetadata::TypeQualifiers typeQualifiers;

    KernelArgMetadata::AccessQualifier getAccessQualifier() const {
        return static_cast<KernelArgMetadata::AccessQualifier>(accessQualifier);
    }

    KernelArgMetadata::AddressSpaceQualifier getAddressQualifier() const {
        return static_cast<KernelArgMetadata::AddressSpaceQualifier>(addressQualifier);
    }
};

namespace {
static constexpr auto ArgTypeMetadataSize = sizeof(ArgTypeTraits);
static_assert(ArgTypeMetadataSize <= 4, "Keep it small");
} // namespace

struct ArgTypeMetadataExtended {
    std::string argName;
    std::string type;
    std::string accessQualifier;
    std::string addressQualifier;
    std::string typeQualifiers;
};

} // namespace NEO

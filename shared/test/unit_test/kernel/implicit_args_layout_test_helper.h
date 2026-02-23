/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/kernel/implicit_args_base.h"

#include "gtest/gtest.h"
#include "implicit_args.h"

#include <cstddef>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using OffsetMap = std::vector<std::pair<std::string, size_t>>;

inline OffsetMap getImplicitArgsV0ExpectedOffsets() {
    return {
        {"structSize", offsetof(NEO::ImplicitArgsV0, header) + offsetof(NEO::ImplicitArgsHeader, structSize)},
        {"structVersion", offsetof(NEO::ImplicitArgsV0, header) + offsetof(NEO::ImplicitArgsHeader, structVersion)},
        {"numWorkDim", offsetof(NEO::ImplicitArgsV0, numWorkDim)},
        {"simdWidth", offsetof(NEO::ImplicitArgsV0, simdWidth)},
        {"localSizeX", offsetof(NEO::ImplicitArgsV0, localSizeX)},
        {"localSizeY", offsetof(NEO::ImplicitArgsV0, localSizeY)},
        {"localSizeZ", offsetof(NEO::ImplicitArgsV0, localSizeZ)},
        {"globalSizeX", offsetof(NEO::ImplicitArgsV0, globalSizeX)},
        {"globalSizeY", offsetof(NEO::ImplicitArgsV0, globalSizeY)},
        {"globalSizeZ", offsetof(NEO::ImplicitArgsV0, globalSizeZ)},
        {"printfBufferPtr", offsetof(NEO::ImplicitArgsV0, printfBufferPtr)},
        {"globalOffsetX", offsetof(NEO::ImplicitArgsV0, globalOffsetX)},
        {"globalOffsetY", offsetof(NEO::ImplicitArgsV0, globalOffsetY)},
        {"globalOffsetZ", offsetof(NEO::ImplicitArgsV0, globalOffsetZ)},
        {"localIdTablePtr", offsetof(NEO::ImplicitArgsV0, localIdTablePtr)},
        {"groupCountX", offsetof(NEO::ImplicitArgsV0, groupCountX)},
        {"groupCountY", offsetof(NEO::ImplicitArgsV0, groupCountY)},
        {"groupCountZ", offsetof(NEO::ImplicitArgsV0, groupCountZ)},
        {"padding0", offsetof(NEO::ImplicitArgsV0, padding0)},
        {"rtGlobalBufferPtr", offsetof(NEO::ImplicitArgsV0, rtGlobalBufferPtr)},
        {"assertBufferPtr", offsetof(NEO::ImplicitArgsV0, assertBufferPtr)},
        {"reserved", offsetof(NEO::ImplicitArgsV0, reserved)},
    };
}

inline OffsetMap getImplicitArgsV1ExpectedOffsets() {
    return {
        {"structSize", offsetof(NEO::ImplicitArgsV1, header) + offsetof(NEO::ImplicitArgsHeader, structSize)},
        {"structVersion", offsetof(NEO::ImplicitArgsV1, header) + offsetof(NEO::ImplicitArgsHeader, structVersion)},
        {"numWorkDim", offsetof(NEO::ImplicitArgsV1, numWorkDim)},
        {"simdWidth", offsetof(NEO::ImplicitArgsV1, simdWidth)},
        {"localSizeX", offsetof(NEO::ImplicitArgsV1, localSizeX)},
        {"localSizeY", offsetof(NEO::ImplicitArgsV1, localSizeY)},
        {"localSizeZ", offsetof(NEO::ImplicitArgsV1, localSizeZ)},
        {"globalSizeX", offsetof(NEO::ImplicitArgsV1, globalSizeX)},
        {"globalSizeY", offsetof(NEO::ImplicitArgsV1, globalSizeY)},
        {"globalSizeZ", offsetof(NEO::ImplicitArgsV1, globalSizeZ)},
        {"printfBufferPtr", offsetof(NEO::ImplicitArgsV1, printfBufferPtr)},
        {"globalOffsetX", offsetof(NEO::ImplicitArgsV1, globalOffsetX)},
        {"globalOffsetY", offsetof(NEO::ImplicitArgsV1, globalOffsetY)},
        {"globalOffsetZ", offsetof(NEO::ImplicitArgsV1, globalOffsetZ)},
        {"localIdTablePtr", offsetof(NEO::ImplicitArgsV1, localIdTablePtr)},
        {"groupCountX", offsetof(NEO::ImplicitArgsV1, groupCountX)},
        {"groupCountY", offsetof(NEO::ImplicitArgsV1, groupCountY)},
        {"groupCountZ", offsetof(NEO::ImplicitArgsV1, groupCountZ)},
        {"padding0", offsetof(NEO::ImplicitArgsV1, padding0)},
        {"rtGlobalBufferPtr", offsetof(NEO::ImplicitArgsV1, rtGlobalBufferPtr)},
        {"assertBufferPtr", offsetof(NEO::ImplicitArgsV1, assertBufferPtr)},
        {"scratchPtr", offsetof(NEO::ImplicitArgsV1, scratchPtr)},
        {"syncBufferPtr", offsetof(NEO::ImplicitArgsV1, syncBufferPtr)},
        {"enqueuedLocalSizeX", offsetof(NEO::ImplicitArgsV1, enqueuedLocalSizeX)},
        {"enqueuedLocalSizeY", offsetof(NEO::ImplicitArgsV1, enqueuedLocalSizeY)},
        {"enqueuedLocalSizeZ", offsetof(NEO::ImplicitArgsV1, enqueuedLocalSizeZ)},
    };
}

OffsetMap getImplicitArgsV2ExpectedOffsets();

struct MemberInfo {
    std::string type; // "uint8_t", "uint16_t", "uint32_t", "uint64_t"
    std::string name;
    size_t typeSize;
    size_t arrayCount; // 1 for scalars, N e.g. reserved[16]
    size_t memberSize;
};

inline std::vector<MemberInfo> parseStructString(const char *str) {
    std::vector<MemberInfo> members;
    std::istringstream stream(str);
    std::string line;
    std::regex memberRegex(R"(^\s*(uint8_t|uint16_t|uint32_t|uint64_t)\s+(\w+)(\[(\d+)\])?\s*;\s*$)");

    while (std::getline(stream, line)) {
        if (line.find("struct ") == 0 || line.empty()) {
            continue;
        }

        std::smatch match;
        if (std::regex_match(line, match, memberRegex)) {
            MemberInfo mi;
            mi.type = match[1].str();
            mi.name = match[2].str();

            if (mi.type == "uint8_t") {
                mi.typeSize = sizeof(uint8_t);
            } else if (mi.type == "uint16_t") {
                mi.typeSize = sizeof(uint16_t);
            } else if (mi.type == "uint32_t") {
                mi.typeSize = sizeof(uint32_t);
            } else if (mi.type == "uint64_t") {
                mi.typeSize = sizeof(uint64_t);
            }

            mi.arrayCount = match[4].matched ? std::stoul(match[4].str()) : 1;
            mi.memberSize = mi.typeSize * mi.arrayCount;
            members.push_back(mi);
        }
    }
    return members;
}

template <typename ImplicitArgsType>
void verifyStructLayoutMatchesPrintedString(const OffsetMap &expectedOffsets) {

    // If below expectations fail, probably implicit arg layout was changed
    // without changing the printed layout string
    // please update getStructAsString function for a given implicit arg struct

    auto members = parseStructString(ImplicitArgsType::getStructAsString());
    ASSERT_FALSE(members.empty());

    ASSERT_EQ(members.size(), expectedOffsets.size());

    size_t fromPrintedStringOffset = 0;
    for (size_t i = 0; i < members.size(); ++i) {
        const auto &fromPrintedString = members[i];
        const auto &fromStruct = expectedOffsets[i];

        EXPECT_EQ(fromPrintedString.name, fromStruct.first);
        EXPECT_EQ(fromPrintedStringOffset, fromStruct.second);

        fromPrintedStringOffset += fromPrintedString.memberSize;
    }
}

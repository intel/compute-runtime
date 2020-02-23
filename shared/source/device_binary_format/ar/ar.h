/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

namespace NEO {
namespace Ar {

static constexpr ConstStringRef arMagic = "!<arch>\n";
static constexpr ConstStringRef arFileEntryTrailingMagic = "\x60\x0A";

struct ArFileEntryHeader {
    char identifier[16] = {'/', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char fileModificationTimestamp[12] = {'0', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char ownerId[6] = {'0', ' ', ' ', ' ', ' ', ' '};
    char groupId[6] = {'0', ' ', ' ', ' ', ' ', ' '};
    char fileMode[8] = {'6', '4', '4', ' ', ' ', ' ', ' ', ' '};
    char fileSizeInBytes[10] = {'0', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char trailingMagic[2] = {arFileEntryTrailingMagic[0], arFileEntryTrailingMagic[1]};
};
static_assert(60U == sizeof(ArFileEntryHeader), "");

namespace SpecialFileNames {
static constexpr ConstStringRef longFileNamesFile = "//";
static const char longFileNamePrefix = '/';
static const char fileNameTerminator = '/';
} // namespace SpecialFileNames

} // namespace Ar

} // namespace NEO

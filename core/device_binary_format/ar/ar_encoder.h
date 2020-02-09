/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/device_binary_format/ar/ar.h"
#include "core/utilities/arrayref.h"
#include "core/utilities/const_stringref.h"

#include <cstring>
#include <vector>

namespace NEO {
namespace Ar {

struct ArEncoder {
    ArEncoder(bool padTo8Bytes = false) : padTo8Bytes(padTo8Bytes) {}
    ArFileEntryHeader *appendFileEntry(const ConstStringRef fileName, const ArrayRef<const uint8_t> fileData);
    std::vector<uint8_t> encode() const;

  protected:
    std::vector<uint8_t> fileEntries;
    bool padTo8Bytes = false;
    uint32_t paddingEntry = 0U;
};

} // namespace Ar

} // namespace NEO

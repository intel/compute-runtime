/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

struct ProgramInfo;
struct DeviceInfoKernelPayloadConstants;

namespace PatchTokenBinary {
struct ProgramFromPatchtokens;
}

inline uint64_t readMisalignedUint64(const uint64_t *address) {
    const uint32_t *addressBits = reinterpret_cast<const uint32_t *>(address);
    return static_cast<uint64_t>(static_cast<uint64_t>(addressBits[1]) << 32) | addressBits[0];
}

bool requiresLocalMemoryWindowVA(const PatchTokenBinary::ProgramFromPatchtokens &src);

void populateProgramInfo(ProgramInfo &dst, const PatchTokenBinary::ProgramFromPatchtokens &src, const DeviceInfoKernelPayloadConstants &constants);

} // namespace NEO

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

#ifndef BIT
#define BIT(x) (1u << (x))
#endif

namespace L0::MCL {

using MutableFlags = uint32_t;
struct MutableFlag {
    static constexpr MutableFlags kernelArgument = BIT(0);
    static constexpr MutableFlags groupCount = BIT(1);
    static constexpr MutableFlags groupSize = BIT(2);
    static constexpr MutableFlags globalOffset = BIT(3);
    static constexpr MutableFlags signalEvent = BIT(4);
    static constexpr MutableFlags waitEvents = BIT(5);
    static constexpr MutableFlags kernelInstruction = BIT(6);
    static constexpr MutableFlags all = kernelArgument | groupCount | groupSize | globalOffset |
                                        signalEvent | waitEvents | kernelInstruction;
};

struct MutableCommandIdDescriptor {
    MutableFlags flags = 0;
};

} // namespace L0::MCL

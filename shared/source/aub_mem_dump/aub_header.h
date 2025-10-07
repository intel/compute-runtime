/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
namespace AubMemDump {

struct CmdServicesMemTraceVersion {
    struct SteppingValues {
        enum {
            N = 13,
            O = 14,
            L = 11,
            M = 12,
            B = 1,
            C = 2,
            A = 0,
            F = 5,
            G = 6,
            D = 3,
            E = 4,
            Z = 25,
            X = 23,
            Y = 24,
            R = 17,
            S = 18,
            P = 15,
            Q = 16,
            V = 21,
            W = 22,
            T = 19,
            U = 20,
            J = 9,
            K = 10,
            H = 7,
            I = 8
        };
    };
};

struct CmdServicesMemTraceMemoryCompare {
    struct CompareOperationValues {
        enum {
            CompareEqual = 0,
            CompareNotEqual = 1,
        };
    };
};

struct CmdServicesMemTraceMemoryWrite {
    struct DataTypeHintValues {
        enum {
            TraceNotype = 0,
            TraceBatchBuffer = 1,
        };
    };
};

} // namespace AubMemDump

/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
enum class TransferDirection {
    hostToHost,
    hostToLocal,
    localToHost,
    localToLocal,
};

inline TransferDirection createTransferDirection(bool srcLocal, bool dstLocal) {
    if (srcLocal) {
        if (dstLocal) {
            return TransferDirection::localToLocal;
        } else {
            return TransferDirection::localToHost;
        }
    } else {
        if (dstLocal) {
            return TransferDirection::hostToLocal;
        } else {
            return TransferDirection::hostToHost;
        }
    }
}
} // namespace NEO

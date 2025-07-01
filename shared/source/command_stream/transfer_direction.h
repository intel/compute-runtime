/*
 * Copyright (C) 2023-2025 Intel Corporation
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
    remote,
};

inline TransferDirection createTransferDirection(bool srcLocal, bool dstLocal, bool remoteCopy) {
    if (remoteCopy) {
        return TransferDirection::remote;
    }

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

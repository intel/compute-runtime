/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
enum class TransferDirection {
    HostToHost,
    HostToLocal,
    LocalToHost,
    LocalToLocal,
};

inline TransferDirection createTransferDirection(bool srcLocal, bool dstLocal) {
    if (srcLocal) {
        if (dstLocal) {
            return TransferDirection::LocalToLocal;
        } else {
            return TransferDirection::LocalToHost;
        }
    } else {
        if (dstLocal) {
            return TransferDirection::HostToLocal;
        } else {
            return TransferDirection::HostToHost;
        }
    }
}
} // namespace NEO
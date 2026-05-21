/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programFrontEndPrologue(LinearStream &csr) {
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getFrontEndPrologueSize() const {
    return 0;
}

} // namespace NEO

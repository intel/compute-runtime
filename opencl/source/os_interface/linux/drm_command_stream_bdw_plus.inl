/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"

#include "opencl/source/os_interface/linux/drm_command_stream.h"

namespace NEO {

template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::flushInternal(const BatchBuffer &batchBuffer, const ResidencyContainer &allocationsForResidency) {
    this->processResidency(allocationsForResidency, 0u);
    this->exec(batchBuffer, 0u, static_cast<const OsContextLinux *>(osContext)->getDrmContextIds()[0]);
}

} // namespace NEO

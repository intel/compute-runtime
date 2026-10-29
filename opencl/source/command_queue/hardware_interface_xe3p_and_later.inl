/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

template <>
void HardwareInterface<Family>::dispatchWalkerCommon(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs) {
    dispatchWalker<Family::COMPUTE_WALKER_2>(commandQueue, multiDispatchInfo, csrDependencies, walkerArgs);
}

} // namespace NEO

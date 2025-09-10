/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function.h"

namespace NEO {
template void HostFunctionHelper::programHostFunction<Family>(LinearStream &commandStream, const HostFunctionData &hostFunctionData, uint64_t userHostFunctionAddress, uint64_t userDataAddress);
template void HostFunctionHelper::programHostFunctionUserData<Family>(LinearStream &commandStream, const HostFunctionData &hostFunctionData, uint64_t userHostFunctionAddress, uint64_t userDataAddress);
template void HostFunctionHelper::programSignalHostFunctionStart<Family>(LinearStream &commandStream, const HostFunctionData &hostFunctionData);
template void HostFunctionHelper::programWaitForHostFunctionCompletion<Family>(LinearStream &commandStream, const HostFunctionData &hostFunctionData);
} // namespace NEO

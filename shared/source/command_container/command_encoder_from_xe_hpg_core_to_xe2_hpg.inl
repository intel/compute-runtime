/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"

namespace NEO {

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::setGrfInfo(InterfaceDescriptorType *pInterfaceDescriptor, uint32_t grfCount,
                                              const size_t &sizeCrossThreadData, const size_t &sizePerThreadData,
                                              const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <typename Family>
bool EncodeEnableRayTracing<Family>::is48bResourceNeededForRayTracing() {
    return true;
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeEuSchedulingPolicy(InterfaceDescriptorType *pInterfaceDescriptor, const KernelDescriptor &kernelDesc, int32_t defaultPipelinedThreadArbitrationPolicy) {
}

} // namespace NEO

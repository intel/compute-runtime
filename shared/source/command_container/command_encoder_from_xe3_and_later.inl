/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/pipe_control_args.h"

namespace NEO {

template <typename Family>
void EncodeComputeMode<Family>::programComputeModeCommandWithSynchronization(LinearStream &csr, StateComputeModeProperties &properties, const PipelineSelectArgs &args,
                                                                             bool hasSharedHandles, const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs, bool dcFlush) {
    programComputeModeCommand(csr, properties, rootDeviceEnvironment);
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::setGrfInfo(InterfaceDescriptorType *pInterfaceDescriptor, uint32_t grfCount,
                                              const size_t &sizeCrossThreadData, const size_t &sizePerThreadData,
                                              const RootDeviceEnvironment &rootDeviceEnvironment) {
    using REGISTERS_PER_THREAD = typename InterfaceDescriptorType::REGISTERS_PER_THREAD;

    struct NumGrfsForIdd {
        bool operator==(uint32_t grfCount) const { return this->grfCount == grfCount; }
        uint32_t grfCount;
        REGISTERS_PER_THREAD valueForIdd;
    };

    const std::array<NumGrfsForIdd, 8> validNumGrfsForIdd{{{32u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_32},
                                                           {64u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_64},
                                                           {96u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_96},
                                                           {128u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_128},
                                                           {160u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_160},
                                                           {192u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_192},
                                                           {256u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_256},
                                                           {512u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_512}}};

    const auto &productHelper = rootDeviceEnvironment.template getHelper<ProductHelper>();
    const auto supportedNumGrfs = productHelper.getSupportedNumGrfs(rootDeviceEnvironment.getReleaseHelper());

    for (const auto &supportedNumGrf : supportedNumGrfs) {
        if (grfCount <= supportedNumGrf) {
            auto value = std::find(validNumGrfsForIdd.begin(), validNumGrfsForIdd.end(), supportedNumGrf);
            if (value != validNumGrfsForIdd.end()) {
                pInterfaceDescriptor->setRegistersPerThread(value->valueForIdd);
                return;
            }
        }
    }

    UNRECOVERABLE_IF(true); // out of expected range
}

} // namespace NEO

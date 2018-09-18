/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/hw_helper.h"

namespace OCLRT {
class Kernel;
class Device;
class GraphicsAllocation;
struct MultiDispatchInfo;

class PreemptionHelper {
  public:
    template <typename CmdFamily>
    using INTERFACE_DESCRIPTOR_DATA = typename CmdFamily::INTERFACE_DESCRIPTOR_DATA;

    static PreemptionMode taskPreemptionMode(Device &device, Kernel *kernel);
    static PreemptionMode taskPreemptionMode(Device &device, const MultiDispatchInfo &multiDispatchInfo);
    static bool allowThreadGroupPreemption(Kernel *kernel, const WorkaroundTable *waTable);
    static bool allowMidThreadPreemption(Kernel *kernel, Device &device);
    static void adjustDefaultPreemptionMode(RuntimeCapabilityTable &deviceCapabilities, bool allowMidThread, bool allowThreadGroup, bool allowMidBatch);

    template <typename GfxFamily>
    static size_t getRequiredPreambleSize(const Device &device);

    template <typename GfxFamily>
    static void programPreamble(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr);

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSize(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode);

    template <typename GfxFamily>
    static void programCmdStream(LinearStream &cmdStream, PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                 GraphicsAllocation *preemptionCsr, Device &device);

    template <typename GfxFamily>
    static size_t getPreemptionWaCsSize(const Device &device);

    template <typename GfxFamily>
    static void applyPreemptionWaCmdsBegin(LinearStream *pCommandStream, const Device &device);

    template <typename GfxFamily>
    static void applyPreemptionWaCmdsEnd(LinearStream *pCommandStream, const Device &device);

    static PreemptionMode getDefaultPreemptionMode(const HardwareInfo &hwInfo);

    template <typename GfxFamily>
    static void programInterfaceDescriptorDataPreemption(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode);
};

template <typename GfxFamily>
struct PreemptionConfig {
    static const uint32_t mmioAddress;
    static const uint32_t maskVal;
    static const uint32_t maskShift;
    static const uint32_t mask;

    static const uint32_t threadGroupVal;
    static const uint32_t cmdLevelVal;
    static const uint32_t midThreadVal;
};

} // namespace OCLRT

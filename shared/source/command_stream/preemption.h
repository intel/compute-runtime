/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/hw_info.h"

#include "sku_info.h"

namespace NEO {
class Kernel;
class Device;
class GraphicsAllocation;
struct MultiDispatchInfo;

struct PreemptionFlags {
    PreemptionFlags() {
        data = 0;
    }
    union {
        struct {
            uint32_t disabledMidThreadPreemptionKernel : 1;
            uint32_t vmeKernel : 1;
            uint32_t deviceSupportsVmePreemption : 1;
            uint32_t disablePerCtxtPreemptionGranularityControl : 1;
            uint32_t usesFencesForReadWriteImages : 1;
            uint32_t disableLSQCROPERFforOCL : 1;
            uint32_t schedulerKernel : 1;
            uint32_t reserved : 25;
        } flags;
        uint32_t data;
    };
};

class PreemptionHelper {
  public:
    template <typename CmdFamily>
    using INTERFACE_DESCRIPTOR_DATA = typename CmdFamily::INTERFACE_DESCRIPTOR_DATA;

    static PreemptionMode taskPreemptionMode(PreemptionMode devicePreemptionMode, const PreemptionFlags &flags);
    static PreemptionMode taskPreemptionMode(Device &device, const MultiDispatchInfo &multiDispatchInfo);
    static bool allowThreadGroupPreemption(const PreemptionFlags &flags);
    static bool allowMidThreadPreemption(const PreemptionFlags &flags);
    static void adjustDefaultPreemptionMode(RuntimeCapabilityTable &deviceCapabilities, bool allowMidThread, bool allowThreadGroup, bool allowMidBatch);

    static void setPreemptionLevelFlags(PreemptionFlags &flags, Device &device, Kernel *kernel);

    template <typename GfxFamily>
    static size_t getRequiredPreambleSize(const Device &device);
    template <typename GfxFamily>
    static size_t getRequiredStateSipCmdSize(const Device &device);

    template <typename GfxFamily>
    static void programCsrBaseAddress(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr);

    template <typename GfxFamily>
    static void programStateSip(LinearStream &preambleCmdStream, Device &device);

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSize(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode);

    template <typename GfxFamily>
    static void programCmdStream(LinearStream &cmdStream, PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                 GraphicsAllocation *preemptionCsr);

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
    static const uint32_t mask;

    static const uint32_t threadGroupVal;
    static const uint32_t cmdLevelVal;
    static const uint32_t midThreadVal;
};

} // namespace NEO

/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NEO {
class LinearStream;
enum PreemptionMode : uint32_t;
struct HardwareInfo;
struct RootDeviceEnvironment;
class Device;
class GraphicsAllocation;
struct KernelDescriptor;
struct RuntimeCapabilityTable;
class OsContext;

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
            uint32_t reserved : 26;
        } flags;
        uint32_t data;
    };
};

class PreemptionHelper {
  public:
    template <typename CmdFamily>
    using INTERFACE_DESCRIPTOR_DATA = typename CmdFamily::INTERFACE_DESCRIPTOR_DATA;

    static PreemptionMode taskPreemptionMode(PreemptionMode devicePreemptionMode, const PreemptionFlags &flags);
    static bool allowThreadGroupPreemption(const PreemptionFlags &flags);
    static bool allowMidThreadPreemption(const PreemptionFlags &flags);
    static void adjustDefaultPreemptionMode(RuntimeCapabilityTable &deviceCapabilities, bool allowMidThread, bool allowThreadGroup, bool allowMidBatch);
    static PreemptionFlags createPreemptionLevelFlags(Device &device, const KernelDescriptor *kernelDescriptor);

    template <typename GfxFamily>
    static size_t getRequiredPreambleSize(const Device &device);
    template <typename GfxFamily>
    static size_t getRequiredStateSipCmdSize(Device &device, bool isRcs);

    template <typename GfxFamily>
    static void programCsrBaseAddress(LinearStream &preambleCmdStream, Device &device, const GraphicsAllocation *preemptionCsr);

    template <typename GfxFamily>
    static void programStateSip(LinearStream &preambleCmdStream, Device &device, OsContext *context);

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSize(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode);

    template <typename GfxFamily>
    static void programCmdStream(LinearStream &cmdStream, PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                 GraphicsAllocation *preemptionCsr);

    static PreemptionMode getDefaultPreemptionMode(const HardwareInfo &hwInfo);

    template <typename GfxFamily, typename InterfaceDescriptorType>
    static void programInterfaceDescriptorDataPreemption(InterfaceDescriptorType *idd, PreemptionMode preemptionMode);

  protected:
    template <typename GfxFamily>
    static void programCsrBaseAddressCmd(LinearStream &preambleCmdStream, const GraphicsAllocation *preemptionCsr);

    template <typename GfxFamily>
    static void programStateSipCmd(LinearStream &preambleCmdStream, GraphicsAllocation *sipAllocation, bool useFullAddress);
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

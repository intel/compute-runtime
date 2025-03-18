/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace NEO {

enum class MemoryCompressionState;

class GmmHelper;
class IndirectHeap;
class LinearStream;
struct DispatchFlags;
struct HardwareInfo;
struct StateBaseAddressProperties;

template <typename GfxFamily>
struct StateBaseAddressHelperArgs {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    uint64_t generalStateBaseAddress = 0;
    uint64_t indirectObjectHeapBaseAddress = 0;
    uint64_t instructionHeapBaseAddress = 0;
    uint64_t globalHeapsBaseAddress = 0;
    uint64_t surfaceStateBaseAddress = 0;
    uint64_t bindlessSurfaceStateBaseAddress = 0;

    STATE_BASE_ADDRESS *stateBaseAddressCmd = nullptr;

    StateBaseAddressProperties *sbaProperties = nullptr;

    const IndirectHeap *dsh = nullptr;
    const IndirectHeap *ioh = nullptr;
    const IndirectHeap *ssh = nullptr;
    GmmHelper *gmmHelper = nullptr;

    uint32_t statelessMocsIndex = 0;
    uint32_t l1CachePolicy = 0;
    uint32_t l1CachePolicyDebuggerActive = 0;
    MemoryCompressionState memoryCompressionState;

    bool setInstructionStateBaseAddress = false;
    bool setGeneralStateBaseAddress = false;
    bool useGlobalHeapsBaseAddress = false;
    bool isMultiOsContextCapable = false;
    bool areMultipleSubDevicesInContext = false;
    bool overrideSurfaceStateBaseAddress = false;
    bool isDebuggerActive = false;
    bool doubleSbaWa = false;
    bool heaplessModeEnabled = false;
};

template <typename GfxFamily>
struct StateBaseAddressHelper {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    static STATE_BASE_ADDRESS *getSpaceForSbaCmd(LinearStream &cmdStream);

    static void programStateBaseAddress(StateBaseAddressHelperArgs<GfxFamily> &args);
    static void programStateBaseAddressIntoCommandStream(StateBaseAddressHelperArgs<GfxFamily> &args, LinearStream &commandStream);

    static void appendIohParameters(StateBaseAddressHelperArgs<GfxFamily> &args);

    static void appendStateBaseAddressParameters(StateBaseAddressHelperArgs<GfxFamily> &args);

    static void appendExtraCacheSettings(StateBaseAddressHelperArgs<GfxFamily> &args);

    static void programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh, GmmHelper *gmmHelper);

    static void programBindingTableBaseAddress(LinearStream &commandStream, uint64_t baseAddress, uint32_t sizeInPages, GmmHelper *gmmHelper);

    static uint32_t getMaxBindlessSurfaceStates();

    static void programHeaplessStateBaseAddress(STATE_BASE_ADDRESS &sba);
    static size_t getSbaCmdSize();
};
} // namespace NEO

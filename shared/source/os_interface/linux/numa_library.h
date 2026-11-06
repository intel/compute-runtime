/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/os_library_linux.h"
#include "shared/source/os_interface/os_library.h"

#include <climits>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>
namespace NEO {
namespace Linux {

class NumaLibrary {
  public:
    static bool init();
    static bool isLoaded() { return numaLoaded; }
    static bool getMemPolicy(int *mode, std::vector<unsigned long> &nodeMask);
    static std::optional<uint32_t> getSinglePreferredNode();
    static int getDistance(uint32_t fromNode, uint32_t toNode);

    static constexpr int mpolDefault = 0;
    static constexpr int mpolPreferred = 1;
    static constexpr int mpolBind = 2;
    static constexpr int mpolLocal = 4;
    static constexpr int mpolPreferredMany = 5;
    static constexpr int mpolFStaticNodes = 1 << 15;
    static constexpr int mpolFRelativeNodes = 1 << 14;
    static constexpr int mpolFNumaBalancing = 1 << 13;
    static constexpr int mpolModeFlags = mpolFStaticNodes | mpolFRelativeNodes | mpolFNumaBalancing;
    static constexpr uint32_t bitsPerNodeMaskElement = sizeof(unsigned long) * CHAR_BIT;

  protected:
    static constexpr const char *numaLibNameStr = "libnuma.so.1";
    static constexpr const char *procGetMemPolicyStr = "get_mempolicy";
    static constexpr const char *procNumaAvailableStr = "numa_available";
    static constexpr const char *procNumaMaxNodeStr = "numa_max_node";
    static constexpr const char *procNumaDistanceStr = "numa_distance";
    static constexpr const char *procNumaNumPossibleNodesStr = "numa_num_possible_nodes";
    static constexpr const char *procNumaGetRunNodeMaskStr = "numa_get_run_node_mask";
    static constexpr const char *procNumaBitmaskFreeStr = "numa_bitmask_free";

    // Layout of struct bitmask from numa.h.
    struct NumaBitmask {
        unsigned long size;
        unsigned long *maskp;
    };

    using GetMemPolicyPtr = std::add_pointer<long(int *, unsigned long[], unsigned long, void *, unsigned long)>::type;
    using NumaAvailablePtr = std::add_pointer<int(void)>::type;
    using NumaMaxNodePtr = std::add_pointer<int(void)>::type;
    using NumaDistancePtr = std::add_pointer<int(int, int)>::type;
    using NumaNumPossibleNodesPtr = std::add_pointer<int(void)>::type;
    using NumaGetRunNodeMaskPtr = std::add_pointer<NumaBitmask *(void)>::type;
    using NumaBitmaskFreePtr = std::add_pointer<void(NumaBitmask *)>::type;

    static std::optional<uint32_t> getSingleNodeInMask(std::span<const unsigned long> nodeMask);
    static std::optional<uint32_t> getSingleRunNode();

    static std::unique_ptr<NEO::OsLibrary> osLibrary;
    static GetMemPolicyPtr getMemPolicyFunction;
    static NumaAvailablePtr numaAvailableFunction;
    static NumaMaxNodePtr numaMaxNodeFunction;
    static NumaDistancePtr numaDistanceFunction;
    static NumaNumPossibleNodesPtr numaNumPossibleNodesFunction;
    static NumaGetRunNodeMaskPtr numaGetRunNodeMaskFunction;
    static NumaBitmaskFreePtr numaBitmaskFreeFunction;
    static int maxNode;
    static int possibleNodes;
    static bool numaLoaded;
};
} // namespace Linux
} // namespace NEO

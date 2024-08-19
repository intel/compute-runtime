/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/os_library_linux.h"
#include "shared/source/os_interface/os_library.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
namespace NEO {
namespace Linux {

class NumaLibrary {
  public:
    static bool init();
    static bool isLoaded() { return numaLoaded; }
    static bool getMemPolicy(int *mode, std::vector<unsigned long> &nodeMask);

  protected:
    static constexpr const char *numaLibNameStr = "libnuma.so.1";
    static constexpr const char *procGetMemPolicyStr = "get_mempolicy";
    static constexpr const char *procNumaAvailableStr = "numa_available";
    static constexpr const char *procNumaMaxNodeStr = "numa_max_node";

    using GetMemPolicyPtr = std::add_pointer<long(int *, unsigned long[], unsigned long, void *, unsigned long)>::type;
    using NumaAvailablePtr = std::add_pointer<int(void)>::type;
    using NumaMaxNodePtr = std::add_pointer<int(void)>::type;

    static std::unique_ptr<NEO::OsLibrary> osLibrary;
    static GetMemPolicyPtr getMemPolicyFunction;
    static NumaAvailablePtr numaAvailableFunction;
    static NumaMaxNodePtr numaMaxNodeFunction;
    static int maxNode;
    static bool numaLoaded;
};
} // namespace Linux
} // namespace NEO

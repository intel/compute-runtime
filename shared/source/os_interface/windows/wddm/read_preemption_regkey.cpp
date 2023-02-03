/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/debug_registry_reader.h"

#include <memory>

namespace NEO {

unsigned int readEnablePreemptionRegKey() {
    auto registryReader = std::make_unique<RegistryReader>(false, "System\\CurrentControlSet\\Control\\GraphicsDrivers\\Scheduler");
    return static_cast<unsigned int>(registryReader->getSetting("EnablePreemption", 1));
}

} // namespace NEO

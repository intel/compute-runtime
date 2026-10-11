/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/wddm/adapter_factory.h"

#include <string>
#include <string_view>

namespace NEO {

[[nodiscard]] std::string queryAdapterDriverStorePath(const Gdi &gdi, D3DKMT_HANDLE adapter);
[[nodiscard]] std::string queryAdapterRegistry(const Gdi &gdi, D3DKMT_HANDLE adapter, std::wstring_view valueName);

} // namespace NEO

/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/wddm/adapter_factory.h"

#include <string>

namespace NEO {

std::wstring queryAdapterDriverStorePath(const Gdi &gdi, D3DKMT_HANDLE adapter);

} // namespace NEO

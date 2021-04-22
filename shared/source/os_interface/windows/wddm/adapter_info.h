/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

typedef unsigned int D3DKMT_HANDLE;

namespace NEO {

class Gdi;

std::wstring queryAdapterDriverStorePath(const Gdi &gdi, D3DKMT_HANDLE adapter);

} // namespace NEO

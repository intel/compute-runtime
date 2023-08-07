/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/pin/pin.h"

namespace L0 {

typedef uint32_t (*OpenGTPin_fn)(void *gtPinInit);

const std::string PinContext::gtPinLibraryFilename = "libgtpin.so";

} // namespace L0

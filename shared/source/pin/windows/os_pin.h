/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/pin/pin.h"

namespace NEO {

typedef uint32_t(__fastcall *OpenGTPin_fn)(void *gtPinInit);
const std::string PinContext::gtPinLibraryFilename = "gtpin.dll";

} // namespace NEO
/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_callbacks.h"

namespace NEO {
NotifyAubCaptureFunc notifyAubCaptureFuncFactory[NEO::maxCoreEnumValue]{};
WriteL3AddressFunc writeL3AddressFuncFactory[NEO::maxCoreEnumValue]{};
} // namespace NEO

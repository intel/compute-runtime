/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
enum GFX3DSTATE_SURFACEFORMAT : unsigned short;
class SupportedMediaFormatsHelper {
  public:
    static bool isMediaFormatSupported(GFX3DSTATE_SURFACEFORMAT format);
};
} // namespace NEO
/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void L0GfxCoreHelperHw<Family>::adjustBuiltInImageModeToBindful(NEO::BuiltIn::AddressingMode &mode) const {
    mode.imageMode = NEO::BuiltIn::AddressingMode::ImageMode::bindful;
}

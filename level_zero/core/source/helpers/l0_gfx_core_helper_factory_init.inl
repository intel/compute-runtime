/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void populateFactoryTable<L0GfxCoreHelperHw<Family>>() {
    extern createL0GfxCoreHelperFunctionType l0GfxCoreHelperFactory[IGFX_MAX_CORE];
    l0GfxCoreHelperFactory[gfxCore] = L0GfxCoreHelperHw<Family>::create;
}

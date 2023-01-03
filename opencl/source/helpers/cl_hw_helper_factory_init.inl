/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void populateFactoryTable<ClGfxCoreHelperHw<Family>>() {
    extern createClGfxCoreHelperFunctionType clGfxCoreHelperFactory[IGFX_MAX_CORE];
    clGfxCoreHelperFactory[gfxCore] = ClGfxCoreHelperHw<Family>::create;
}

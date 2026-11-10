/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void populateFactoryTable<GfxCoreHelperHw<Family>>() {
    extern GfxCoreHelperCreateFunctionType gfxCoreHelperFactory[NEO::maxCoreEnumValue];
    gfxCoreHelperFactory[Family::gfxCoreFamily] = GfxCoreHelperHw<Family>::create;
}

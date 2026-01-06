/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void populateFactoryTable<L0GfxCoreHelperHw<Family>>() {
    extern createL0GfxCoreHelperFunctionType l0GfxCoreHelperFactory[NEO::maxCoreEnumValue];
    l0GfxCoreHelperFactory[gfxCore] = L0GfxCoreHelperHw<Family>::create;
}

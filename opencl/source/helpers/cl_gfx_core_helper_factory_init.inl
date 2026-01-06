/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void populateFactoryTable<ClGfxCoreHelperHw<Family>>() {
    extern createClGfxCoreHelperFunctionType clGfxCoreHelperFactory[NEO::maxCoreEnumValue];
    clGfxCoreHelperFactory[gfxCore] = ClGfxCoreHelperHw<Family>::create;
}

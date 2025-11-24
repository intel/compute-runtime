/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void populateFactoryTable<BufferHw<Family>>() {
    extern BufferFactoryFuncs bufferFactory[NEO::maxCoreEnumValue];
    bufferFactory[gfxCore].createBufferFunction = BufferHw<Family>::create;
}

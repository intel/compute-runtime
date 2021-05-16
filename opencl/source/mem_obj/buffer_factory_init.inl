/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void populateFactoryTable<BufferHw<Family>>() {
    extern BufferFactoryFuncs bufferFactory[IGFX_MAX_CORE];
    bufferFactory[gfxCore].createBufferFunction = BufferHw<Family>::create;
}

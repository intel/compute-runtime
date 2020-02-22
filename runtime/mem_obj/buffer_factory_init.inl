/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void populateFactoryTable<BufferHw<Family>>() {
    extern BufferFuncs bufferFactory[IGFX_MAX_CORE];
    bufferFactory[gfxCore].createBufferFunction = BufferHw<Family>::create;
}

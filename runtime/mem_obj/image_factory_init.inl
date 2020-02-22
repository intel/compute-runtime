/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template class ImageHw<Family>;
template <>
void populateFactoryTable<ImageHw<Family>>() {
    extern ImageFuncs imageFactory[IGFX_MAX_CORE];
    imageFactory[gfxCore].createImageFunction = ImageHw<Family>::create;
}

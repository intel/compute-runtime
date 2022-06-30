/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

template class ImageHw<Family>;
template <>
void populateFactoryTable<ImageHw<Family>>() {
    extern ImageFactoryFuncs imageFactory[IGFX_MAX_CORE];
    imageFactory[gfxCore].createImageFunction = ImageHw<Family>::create;
}
} // namespace NEO

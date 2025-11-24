/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

template class ImageHw<Family>;
template <>
void populateFactoryTable<ImageHw<Family>>() {
    extern ImageFactoryFuncs imageFactory[NEO::maxCoreEnumValue];
    imageFactory[gfxCore].createImageFunction = ImageHw<Family>::create;
}
} // namespace NEO

/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <>
void EncodeSurfaceState<Family>::setCoherencyType(Family::RENDER_SURFACE_STATE *surfaceState, Family::RENDER_SURFACE_STATE::COHERENCY_TYPE coherencyType) {
}
} // namespace NEO
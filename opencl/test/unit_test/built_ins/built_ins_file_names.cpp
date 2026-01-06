/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string>
#include <vector>

namespace NEO {

std::vector<std::string> builtInFileNames = {
    "aux_translation.builtin_kernel",
    "copy_buffer_to_buffer.builtin_kernel",
    "fill_buffer.builtin_kernel",
    "copy_buffer_rect.builtin_kernel",
    "copy_kernel_timestamps.builtin_kernel"};

std::vector<std::string> imageBuiltInFileNames = {
    "fill_image1d.builtin_kernel",
    "fill_image2d.builtin_kernel",
    "fill_image3d.builtin_kernel",
    "copy_image_to_image1d.builtin_kernel",
    "copy_image_to_image2d.builtin_kernel",
    "copy_image_to_image3d.builtin_kernel",
    "copy_buffer_to_image3d.builtin_kernel",
    "copy_image3d_to_buffer.builtin_kernel",
    "fill_image1d_buffer.builtin_kernel"};

} // namespace NEO

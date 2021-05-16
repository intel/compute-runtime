/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string>
#include <vector>

namespace NEO {

std::vector<std::string> builtInFileNames = {
    "test_files/aux_translation.builtin_kernel",
    "test_files/copy_buffer_to_buffer.builtin_kernel",
    "test_files/fill_buffer.builtin_kernel",
    "test_files/copy_buffer_rect.builtin_kernel",
    "test_files/copy_kernel_timestamps.builtin_kernel"};

std::vector<std::string> imageBuiltInFileNames = {
    "test_files/fill_image1d.builtin_kernel",
    "test_files/fill_image2d.builtin_kernel",
    "test_files/fill_image3d.builtin_kernel",
    "test_files/copy_image_to_image1d.builtin_kernel",
    "test_files/copy_image_to_image2d.builtin_kernel",
    "test_files/copy_image_to_image3d.builtin_kernel",
    "test_files/copy_buffer_to_image3d.builtin_kernel",
    "test_files/copy_image3d_to_buffer.builtin_kernel"};

} // namespace NEO

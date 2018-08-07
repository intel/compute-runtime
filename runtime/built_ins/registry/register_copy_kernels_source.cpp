/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string>
#include "runtime/built_ins/registry/built_ins_registry.h"

namespace OCLRT {

static RegisterEmbeddedResource registerCopyBufferToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_buffer_to_buffer.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyBufferRectSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferRect,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_buffer_rect.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerFillBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/fill_buffer.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyBufferToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyBufferToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_buffer_to_image3d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyImage3dToBufferSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImage3dToBuffer,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_image3d_to_buffer.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyImageToImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImageToImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_image_to_image1d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyImageToImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImageToImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_image_to_image2d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerCopyImageToImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::CopyImageToImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/copy_image_to_image3d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerFillImage1dSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage1d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/fill_image1d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerFillImage2dSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage2d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/fill_image2d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerFillImage3dSrc(
    createBuiltinResourceName(
        EBuiltInOps::FillImage3d,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/fill_image3d.igdrcl_built_in"
        ));

static RegisterEmbeddedResource registerAuxTranslationSrc(
    createBuiltinResourceName(
        EBuiltInOps::AuxTranslation,
        BuiltinCode::getExtension(BuiltinCode::ECodeType::Source))
        .c_str(),
    std::string(
#include "runtime/built_ins/kernels/aux_translation.igdrcl_built_in"
        ));

} // namespace OCLRT

/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off
using namespace NEO;
using GPGPU_CSR_BASE_ADDRESS          = GenStruct::GPGPU_CSR_BASE_ADDRESS;
// clang-format on

template <>
GPGPU_CSR_BASE_ADDRESS *genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(void *buffer) {
    return matchCommandHeader<GPGPU_CSR_BASE_ADDRESS>(buffer, [](const GPGPU_CSR_BASE_ADDRESS &header) {
        return GPGPU_CSR_BASE_ADDRESS::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
               GPGPU_CSR_BASE_ADDRESS::COMMAND_SUBTYPE_GFXPIPE_COMMON == header.TheStructure.Common.CommandSubtype &&
               GPGPU_CSR_BASE_ADDRESS::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == header.TheStructure.Common._3DCommandOpcode &&
               GPGPU_CSR_BASE_ADDRESS::_3D_COMMAND_SUB_OPCODE_GPGPU_CSR_BASE_ADDRESS == header.TheStructure.Common._3DCommandSubOpcode;
    });
}

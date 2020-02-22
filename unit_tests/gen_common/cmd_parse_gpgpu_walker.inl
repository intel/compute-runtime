/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off
using namespace NEO;
using GPGPU_WALKER                    = GenStruct::GPGPU_WALKER;
using MEDIA_INTERFACE_DESCRIPTOR_LOAD = GenStruct::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
using MEDIA_STATE_FLUSH               = GenStruct::MEDIA_STATE_FLUSH;
using MEDIA_VFE_STATE                 = GenStruct::MEDIA_VFE_STATE;
// clang-format on

template <>
GPGPU_WALKER *genCmdCast<GPGPU_WALKER *>(void *buffer) {
    auto pCmd = reinterpret_cast<GPGPU_WALKER *>(buffer);

    return GPGPU_WALKER::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   GPGPU_WALKER::PIPELINE_MEDIA == pCmd->TheStructure.Common.Pipeline &&
                   GPGPU_WALKER::MEDIA_COMMAND_OPCODE_GPGPU_WALKER == pCmd->TheStructure.Common.MediaCommandOpcode &&
                   GPGPU_WALKER::SUBOPCODE_GPGPU_WALKER_SUBOP == pCmd->TheStructure.Common.Subopcode
               ? pCmd
               : nullptr;
}

template <>
MEDIA_INTERFACE_DESCRIPTOR_LOAD *genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(void *buffer) {
    auto pCmd = reinterpret_cast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(buffer);

    return MEDIA_INTERFACE_DESCRIPTOR_LOAD::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   MEDIA_INTERFACE_DESCRIPTOR_LOAD::PIPELINE_MEDIA == pCmd->TheStructure.Common.Pipeline &&
                   MEDIA_INTERFACE_DESCRIPTOR_LOAD::MEDIA_COMMAND_OPCODE_MEDIA_INTERFACE_DESCRIPTOR_LOAD == pCmd->TheStructure.Common.MediaCommandOpcode &&
                   MEDIA_INTERFACE_DESCRIPTOR_LOAD::SUBOPCODE_MEDIA_INTERFACE_DESCRIPTOR_LOAD_SUBOP == pCmd->TheStructure.Common.Subopcode
               ? pCmd
               : nullptr;
}

template <>
MEDIA_VFE_STATE *genCmdCast<MEDIA_VFE_STATE *>(void *buffer) {
    auto pCmd = reinterpret_cast<MEDIA_VFE_STATE *>(buffer);

    return MEDIA_VFE_STATE::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   MEDIA_VFE_STATE::PIPELINE_MEDIA == pCmd->TheStructure.Common.Pipeline &&
                   MEDIA_VFE_STATE::MEDIA_COMMAND_OPCODE_MEDIA_VFE_STATE == pCmd->TheStructure.Common.MediaCommandOpcode &&
                   MEDIA_VFE_STATE::SUBOPCODE_MEDIA_VFE_STATE_SUBOP == pCmd->TheStructure.Common.Subopcode
               ? pCmd
               : nullptr;
}

template <>
MEDIA_STATE_FLUSH *genCmdCast<MEDIA_STATE_FLUSH *>(void *buffer) {
    auto pCmd = reinterpret_cast<MEDIA_STATE_FLUSH *>(buffer);

    return MEDIA_STATE_FLUSH::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   MEDIA_STATE_FLUSH::PIPELINE_MEDIA == pCmd->TheStructure.Common.Pipeline &&
                   MEDIA_STATE_FLUSH::MEDIA_COMMAND_OPCODE_MEDIA_STATE_FLUSH == pCmd->TheStructure.Common.MediaCommandOpcode &&
                   MEDIA_STATE_FLUSH::SUBOPCODE_MEDIA_STATE_FLUSH_SUBOP == pCmd->TheStructure.Common.Subopcode
               ? pCmd
               : nullptr;
}

template <class T>
bool CmdParse<T>::parseCommandBuffer(GenCmdList &cmds, void *buffer, size_t length) {
    if (!buffer || length % sizeof(uint32_t)) {
        return false;
    }

    void *bufferEnd = reinterpret_cast<uint8_t *>(buffer) + length;
    while (buffer < bufferEnd) {
        size_t length = getCommandLength(buffer);
        if (!length) {
            return false;
        }

        cmds.push_back(buffer);

        buffer = reinterpret_cast<uint32_t *>(buffer) + length;
    }

    return buffer == bufferEnd;
}

// MIDL should have a MSF between it and a previous walker
template <>
template <>
void CmdParse<GenGfxFamily>::validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd) {
    auto itorCurrent = itorBegin;
    auto itorWalker = itorEnd;

    // Find last GPGPU_WALKER prior to itorCmd
    while (itorCurrent != itorEnd) {
        if (genCmdCast<GPGPU_WALKER *>(*itorCurrent)) {
            itorWalker = itorCurrent;
        }

        ++itorCurrent;
    }

    // If we don't find a GPGPU_WALKER, assume the beginning of a cmd list
    itorWalker = itorWalker == itorEnd
                     ? itorBegin
                     : itorWalker;

    // Look for MEDIA_STATE_FLUSH between last GPGPU_WALKER and MIDL.
    auto itorMSF = itorEnd;

    itorCurrent = itorWalker;
    ++itorCurrent;
    while (itorCurrent != itorEnd) {
        if (genCmdCast<MEDIA_STATE_FLUSH *>(*itorCurrent)) {
            itorMSF = itorCurrent;
            break;
        }
        ++itorCurrent;
    }

    ASSERT_FALSE(itorMSF == itorEnd) << "A MEDIA_STATE_FLUSH is required before a MEDIA_INTERFACE_DESCRIPTOR_LOAD.";
}

template <>
template <>
void CmdParse<GenGfxFamily>::validateCommand<STATE_BASE_ADDRESS *>(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd) {
}

// MVFES should have a stalling PC between it and a previous walker
template <>
template <>
void CmdParse<GenGfxFamily>::validateCommand<MEDIA_VFE_STATE *>(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd) {
    auto itorCurrent = itorBegin;
    auto itorWalker = itorEnd;

    // Find last GPGPU_WALKER prior to itorCmd
    while (itorCurrent != itorEnd) {
        if (genCmdCast<GPGPU_WALKER *>(*itorCurrent)) {
            itorWalker = itorCurrent;
        }

        ++itorCurrent;
    }

    // If we don't find a GPGPU_WALKER, assume the beginning of a cmd list
    itorWalker = itorWalker == itorEnd
                     ? itorBegin
                     : itorWalker;

    // Look for PIPE_CONTROL between last GPGPU_WALKER and MVFES.
    itorCurrent = itorWalker;
    ++itorCurrent;
    while (itorCurrent != itorEnd) {
        if (genCmdCast<PIPE_CONTROL *>(*itorCurrent)) {
            auto pPC = genCmdCast<PIPE_CONTROL *>(*itorCurrent);
            if (pPC->getCommandStreamerStallEnable()) {
                return;
            }
        }
        ++itorCurrent;
    }

    ASSERT_TRUE(false) << "A PIPE_CONTROL w/ CS stall is required before a MEDIA_VFE_STATE.";
}

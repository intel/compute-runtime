/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off
using namespace NEO;
using MI_SET_PREDICATE                  = GenStruct::MI_SET_PREDICATE;
// clang-format on

template <>
MI_SET_PREDICATE *genCmdCast<MI_SET_PREDICATE *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_SET_PREDICATE *>(buffer);

    return MI_SET_PREDICATE::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_SET_PREDICATE::MI_COMMAND_OPCODE_MI_SET_PREDICATE == pCmd->TheStructure.Common.MiCommandOpcode
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

template <>
template <>
void CmdParse<GenGfxFamily>::validateCommand<STATE_BASE_ADDRESS *>(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd) {
}

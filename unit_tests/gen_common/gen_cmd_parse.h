/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hw_cmds.h"

#include <list>
#include <vector>

typedef std::list<void *> GenCmdList;

template <typename Type>
Type genCmdCast(void *cmd);

template <typename Type>
static inline GenCmdList::iterator find(GenCmdList::iterator itorStart, GenCmdList::const_iterator itorEnd) {
    GenCmdList::iterator itor = itorStart;
    while (itor != itorEnd) {
        if (genCmdCast<Type>(*itor))
            break;
        ++itor;
    }
    return itor;
}

template <typename CommandToFind>
static inline std::vector<GenCmdList::iterator> findAll(GenCmdList::iterator commandListStart, GenCmdList::const_iterator commandListEnd) {
    std::vector<GenCmdList::iterator> matchedCommands;
    GenCmdList::iterator currentCommand = commandListStart;
    while (currentCommand != commandListEnd) {
        if (genCmdCast<CommandToFind>(*currentCommand)) {
            matchedCommands.push_back(currentCommand);
        }
        ++currentCommand;
    }
    return matchedCommands;
}

template <typename FamilyType>
static inline GenCmdList::iterator findMmio(GenCmdList::iterator itorStart, GenCmdList::const_iterator itorEnd, uint32_t regOffset) {
    GenCmdList::iterator itor = itorStart;
    while (itor != itorEnd) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(*itor);
        if (cmd && cmd->getRegisterOffset() == regOffset)
            break;
        ++itor;
    }
    return itor;
}

template <typename FamilyType>
static inline size_t countMmio(GenCmdList::iterator itorStart, GenCmdList::const_iterator itorEnd, uint32_t regOffset) {
    size_t count = 0;
    GenCmdList::iterator itor = itorStart;
    while (itor != itorEnd) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(*itor);
        if (cmd && cmd->getRegisterOffset() == regOffset) {
            ++count;
        }
        ++itor;
    }
    return count;
}

template <typename FamilyType>
static inline typename FamilyType::MI_LOAD_REGISTER_IMM *findMmioCmd(GenCmdList::iterator itorStart, GenCmdList::const_iterator itorEnd, uint32_t regOffset) {
    auto itor = findMmio<FamilyType>(itorStart, itorEnd, regOffset);
    if (itor == itorEnd) {
        return nullptr;
    }
    return reinterpret_cast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(*itor);
}

template <typename Type>
static inline GenCmdList::reverse_iterator reverse_find(GenCmdList::reverse_iterator itorStart, GenCmdList::const_reverse_iterator itorEnd) {
    GenCmdList::reverse_iterator itor = itorStart;
    while (itor != itorEnd) {
        if (genCmdCast<Type>(*itor))
            break;
        ++itor;
    }
    return itor;
}

template <class T>
struct CmdParse : public T {
    static size_t getCommandLength(void *cmd);
    static size_t getCommandLengthHwSpecific(void *cmd);

    static bool parseCommandBuffer(GenCmdList &_cmds, void *_buffer, size_t _length);

    template <typename CmdType>
    static void validateCommand(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd);

    static const char *getCommandName(void *cmd);
    static const char *getCommandNameHwSpecific(void *cmd);
};

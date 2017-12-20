/*
 * Copyright (c) 2017, Intel Corporation
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

#pragma once
#include "runtime/gen_common/hw_cmds.h"
#include <list>

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

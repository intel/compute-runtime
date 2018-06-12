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

#pragma once
#include "runtime/helpers/basic_math.h"

#include <functional>
#include <atomic>
#include <array>
#include <memory>
#include <vector>

namespace OCLRT {

typedef std::function<void(uint64_t addr, size_t size, size_t offset)> PageWalker;
template <class T, uint32_t level, uint32_t bits = 9>
class PageTable {
  public:
    PageTable() {
        entries.fill(nullptr);
    };
    virtual ~PageTable();

    virtual uintptr_t map(uintptr_t vm, size_t size);
    virtual void pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker);

    static const size_t pageSize = 1 << 12;
    static size_t getBits() {
        return T::getBits() + bits;
    }

  protected:
    std::array<T *, 1 << bits> entries;
};

class PTE : public PageTable<void, 0u> {
  public:
    uintptr_t map(uintptr_t vm, size_t size) override;
    void pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker) override;

    static const uint32_t level = 0;
    static const uint32_t bits = 9;
    static const uint32_t initialPage;

  protected:
    static std::atomic<uint32_t> nextPage;
};
class PDE : public PageTable<class PTE, 1> {
};
class PDP : public PageTable<class PDE, 2> {
};
class PML4 : public PageTable<class PDP, 3> {
};
class PDPE : public PageTable<class PDE, 2, 2> {
};
} // namespace OCLRT

/*
 * Copyright (c) 2018, Intel Corporation
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

namespace OCLRT {

template <>
inline uintptr_t PageTable<void, 0, 9>::map(uintptr_t vm, size_t size) {
    return 0;
}

template <>
inline size_t PageTable<void, 0, 9>::getBits() {
    return 9;
}

template <>
inline void PageTable<void, 0, 9>::pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker) {
}

template <>
inline PageTable<void, 0, 9>::~PageTable() {
}

template <class T, uint32_t level, uint32_t bits>
inline uintptr_t PageTable<T, level, bits>::map(uintptr_t vm, size_t size) {
    const size_t shift = T::getBits() + 12;
    const uintptr_t mask = (1 << bits) - 1;
    size_t indexStart = (vm >> shift) & mask;
    size_t indexEnd = ((vm + size - 1) >> shift) & mask;
    uintptr_t res = -1;
    uintptr_t vmMask = (uintptr_t(-1) >> (sizeof(void *) * 8 - shift - bits));
    auto maskedVm = vm & vmMask;

    for (size_t index = indexStart; index <= indexEnd; index++) {
        uintptr_t vmStart = (uintptr_t(1) << shift) * index;
        vmStart = std::max(vmStart, maskedVm);
        uintptr_t vmEnd = (uintptr_t(1) << shift) * (index + 1) - 1;
        vmEnd = std::min(vmEnd, maskedVm + size - 1);

        if (entries[index] == nullptr) {
            entries[index] = new T;
        }
        res = std::min((entries[index])->map(vmStart, vmEnd - vmStart + 1), res);
    }
    return res;
}

template <class T, uint32_t level, uint32_t bits>
inline void PageTable<T, level, bits>::pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker) {
    const size_t shift = T::getBits() + 12;
    const uintptr_t mask = (1 << bits) - 1;
    size_t indexStart = (vm >> shift) & mask;
    size_t indexEnd = ((vm + size - 1) >> shift) & mask;
    uintptr_t vmMask = (uintptr_t(-1) >> (sizeof(void *) * 8 - shift - bits));
    auto maskedVm = vm & vmMask;

    for (size_t index = indexStart; index <= indexEnd; index++) {
        uintptr_t vmStart = (uintptr_t(1) << shift) * index;
        vmStart = std::max(vmStart, maskedVm);
        uintptr_t vmEnd = (uintptr_t(1) << shift) * (index + 1) - 1;
        vmEnd = std::min(vmEnd, maskedVm + size - 1);

        if (entries[index] == nullptr) {
            entries[index] = new T;
        }
        entries[index]->pageWalk(vmStart, vmEnd - vmStart + 1, offset, pageWalker);

        offset += (vmEnd - vmStart + 1);
    }
}

template <class T, uint32_t level, uint32_t bits>
inline PageTable<T, level, bits>::~PageTable() {
    for (auto &e : entries)
        delete e;
}

} // namespace OCLRT

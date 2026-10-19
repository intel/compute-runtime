/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

constexpr size_t hugePageAlignment = 2u * 1024u * 1024u;
constexpr size_t defaultThreshold = hugePageAlignment;
constexpr size_t initialMappingCapacity = 256u;
constexpr const char *thresholdVariableName = "ALIGN2M_THRESHOLD";

struct Mapping {
    void *ptr;
    size_t length;
};

pthread_mutex_t mappingsLock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
Mapping *mappings = nullptr;
size_t mappingsCount = 0u;
size_t mappingsCapacity = 0u;

size_t threshold = defaultThreshold;
bool symbolsResolved = false;
thread_local bool resolvingSymbols __attribute__((tls_model("initial-exec"))) = false;

void *(*realMalloc)(size_t) = nullptr;
void *(*realCalloc)(size_t, size_t) = nullptr;
void *(*realRealloc)(void *, size_t) = nullptr;
void (*realFree)(void *) = nullptr;
int (*realPosixMemalign)(void **, size_t, size_t) = nullptr;
void *(*realAlignedAlloc)(size_t, size_t) = nullptr;
size_t (*realMallocUsableSize)(void *) = nullptr;

void resolveSymbols() {
    if (symbolsResolved || resolvingSymbols) {
        return;
    }
    resolvingSymbols = true;

    realMalloc = reinterpret_cast<void *(*)(size_t)>(dlsym(RTLD_NEXT, "malloc"));
    realCalloc = reinterpret_cast<void *(*)(size_t, size_t)>(dlsym(RTLD_NEXT, "calloc"));
    realRealloc = reinterpret_cast<void *(*)(void *, size_t)>(dlsym(RTLD_NEXT, "realloc"));
    realFree = reinterpret_cast<void (*)(void *)>(dlsym(RTLD_NEXT, "free"));
    realPosixMemalign = reinterpret_cast<int (*)(void **, size_t, size_t)>(dlsym(RTLD_NEXT, "posix_memalign"));
    realAlignedAlloc = reinterpret_cast<void *(*)(size_t, size_t)>(dlsym(RTLD_NEXT, "aligned_alloc"));
    realMallocUsableSize = reinterpret_cast<size_t (*)(void *)>(dlsym(RTLD_NEXT, "malloc_usable_size"));

    resolvingSymbols = false;
    symbolsResolved = (realMalloc != nullptr) && (realCalloc != nullptr) && (realRealloc != nullptr) && (realFree != nullptr);
}

size_t roundUp(size_t value, size_t alignment) {
    if (value > SIZE_MAX - (alignment - 1u)) {
        return 0u;
    }
    return (value + alignment - 1u) & ~(alignment - 1u);
}

uintptr_t toAddress(const void *ptr) {
    return reinterpret_cast<uintptr_t>(ptr);
}

bool isHugePageAligned(const void *ptr) {
    return (toAddress(ptr) & (hugePageAlignment - 1u)) == 0u;
}

size_t lowerBound(uintptr_t address) {
    size_t low = 0u;
    size_t high = mappingsCount;
    while (low < high) {
        size_t middle = low + (high - low) / 2u;
        if (toAddress(mappings[middle].ptr) < address) {
            low = middle + 1u;
        } else {
            high = middle;
        }
    }
    return low;
}

bool growMappings() {
    size_t capacity = (mappingsCapacity == 0u) ? initialMappingCapacity : mappingsCapacity * 2u;
    size_t bytes = capacity * sizeof(Mapping);
    void *memory = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        return false;
    }
    if (mappingsCount != 0u) {
        memcpy(memory, mappings, mappingsCount * sizeof(Mapping));
    }
    if (mappings != nullptr) {
        munmap(mappings, mappingsCapacity * sizeof(Mapping));
    }
    mappings = static_cast<Mapping *>(memory);
    mappingsCapacity = capacity;
    return true;
}

bool insertMapping(void *ptr, size_t length) {
    pthread_mutex_lock(&mappingsLock);
    if ((mappingsCount == mappingsCapacity) && !growMappings()) {
        pthread_mutex_unlock(&mappingsLock);
        return false;
    }
    size_t index = lowerBound(toAddress(ptr));
    memmove(mappings + index + 1u, mappings + index, (mappingsCount - index) * sizeof(Mapping));
    mappings[index].ptr = ptr;
    mappings[index].length = length;
    mappingsCount++;
    pthread_mutex_unlock(&mappingsLock);
    return true;
}

bool findMapping(const void *ptr, size_t &length) {
    if (!isHugePageAligned(ptr)) {
        return false;
    }
    bool found = false;
    pthread_mutex_lock(&mappingsLock);
    size_t index = lowerBound(toAddress(ptr));
    if ((index < mappingsCount) && (mappings[index].ptr == ptr)) {
        length = mappings[index].length;
        found = true;
    }
    pthread_mutex_unlock(&mappingsLock);
    return found;
}

bool takeMapping(const void *ptr, size_t &length) {
    if (!isHugePageAligned(ptr)) {
        return false;
    }
    bool found = false;
    pthread_mutex_lock(&mappingsLock);
    size_t index = lowerBound(toAddress(ptr));
    if ((index < mappingsCount) && (mappings[index].ptr == ptr)) {
        length = mappings[index].length;
        memmove(mappings + index, mappings + index + 1u, (mappingsCount - index - 1u) * sizeof(Mapping));
        mappingsCount--;
        found = true;
    }
    pthread_mutex_unlock(&mappingsLock);
    return found;
}

void *allocateAligned(size_t size, size_t alignment) {
    size_t granularity = (alignment > hugePageAlignment) ? alignment : hugePageAlignment;
    size_t length = roundUp((size == 0u) ? 1u : size, granularity);
    if ((length == 0u) || (length > SIZE_MAX - granularity)) {
        errno = ENOMEM;
        return nullptr;
    }

    size_t mappingLength = length + granularity;
    void *mapping = mmap(nullptr, mappingLength, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        return nullptr;
    }

    uintptr_t base = reinterpret_cast<uintptr_t>(mapping);
    uintptr_t aligned = (base + granularity - 1u) & ~static_cast<uintptr_t>(granularity - 1u);
    if (aligned != base) {
        munmap(mapping, aligned - base);
    }
    size_t tail = (base + mappingLength) - (aligned + length);
    if (tail != 0u) {
        munmap(reinterpret_cast<void *>(aligned + length), tail);
    }

    void *ptr = reinterpret_cast<void *>(aligned);
    int savedErrno = errno;
    madvise(ptr, length, MADV_HUGEPAGE);
    errno = savedErrno;

    if (!insertMapping(ptr, length)) {
        munmap(ptr, length);
        errno = ENOMEM;
        return nullptr;
    }
    return ptr;
}

bool shouldAlign(size_t size) {
    return (size >= threshold) || (realMalloc == nullptr);
}

bool isValidAlignment(size_t alignment) {
    return (alignment != 0u) && ((alignment & (alignment - 1u)) == 0u);
}

void lockBeforeFork() {
    pthread_mutex_lock(&mappingsLock);
}

void unlockAfterFork() {
    pthread_mutex_unlock(&mappingsLock);
}

void resetLockInChild() {
    // a recursive mutex records the parent's TID as owner, so unlocking it in the child fails with EPERM
    pthread_mutex_t unlocked = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    mappingsLock = unlocked;
}

size_t parseThreshold(const char *value, bool &valid) {
    bool hexadecimal = (value[0] == '0') && ((value[1] == 'x') || (value[1] == 'X'));
    char *end = nullptr;
    errno = 0;
    unsigned long parsed = strtoul(value, &end, hexadecimal ? 16 : 10);
    valid = (value[0] >= '0') && (value[0] <= '9') && (errno == 0) && (*end == '\0');
    if (!valid) {
        return defaultThreshold;
    }
    return (parsed == 0u) ? SIZE_MAX : static_cast<size_t>(parsed);
}

__attribute__((constructor)) void initialize() {
    resolveSymbols();
    pthread_atfork(lockBeforeFork, unlockAfterFork, resetLockInChild);

    const char *value = getenv(thresholdVariableName);
    if (value == nullptr) {
        return;
    }

    bool valid = false;
    threshold = parseThreshold(value, valid);
    if (!valid) {
        static const char message[] = "align2m_preload: ALIGN2M_THRESHOLD is not a valid size, using the default\n";
        ssize_t written = write(STDERR_FILENO, message, sizeof(message) - 1u);
        static_cast<void>(written);
    }
}

} // namespace

// NOLINTBEGIN(readability-identifier-naming)
extern "C" {

void *malloc(size_t size) noexcept {
    resolveSymbols();
    if (shouldAlign(size)) {
        return allocateAligned(size, hugePageAlignment);
    }
    return realMalloc(size);
}

void *calloc(size_t count, size_t size) noexcept {
    resolveSymbols();
    if ((count != 0u) && (size > SIZE_MAX / count)) {
        errno = ENOMEM;
        return nullptr;
    }

    size_t total = count * size;
    if (shouldAlign(total) || (realCalloc == nullptr)) {
        return allocateAligned(total, hugePageAlignment);
    }
    return realCalloc(count, size);
}

void free(void *ptr) noexcept {
    resolveSymbols();
    if (ptr == nullptr) {
        return;
    }

    size_t length = 0u;
    if (takeMapping(ptr, length)) {
        munmap(ptr, length);
        return;
    }
    if (realFree != nullptr) {
        realFree(ptr);
    }
}

void *realloc(void *ptr, size_t size) noexcept {
    resolveSymbols();
    if (ptr == nullptr) {
        return malloc(size);
    }

    size_t oldLength = 0u;
    if (!findMapping(ptr, oldLength)) {
        if (shouldAlign(size) && (realMallocUsableSize != nullptr) && (realFree != nullptr)) {
            size_t usable = realMallocUsableSize(ptr);
            void *newPtr = allocateAligned(size, hugePageAlignment);
            if (newPtr == nullptr) {
                return nullptr;
            }
            memcpy(newPtr, ptr, (size < usable) ? size : usable);
            realFree(ptr);
            return newPtr;
        }
        return (realRealloc != nullptr) ? realRealloc(ptr, size) : nullptr;
    }

    if (size == 0u) {
        if (takeMapping(ptr, oldLength)) {
            munmap(ptr, oldLength);
        }
        return nullptr;
    }
    if (roundUp(size, hugePageAlignment) == oldLength) {
        return ptr;
    }

    void *newPtr = shouldAlign(size) ? allocateAligned(size, hugePageAlignment) : realMalloc(size);
    if (newPtr == nullptr) {
        return nullptr;
    }
    memcpy(newPtr, ptr, (size < oldLength) ? size : oldLength);
    if (takeMapping(ptr, oldLength)) {
        munmap(ptr, oldLength);
    }
    return newPtr;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) noexcept {
    resolveSymbols();
    if (!isValidAlignment(alignment) || ((alignment % sizeof(void *)) != 0u)) {
        return EINVAL;
    }

    if (shouldAlign(size) || (realPosixMemalign == nullptr)) {
        int callerErrno = errno;
        void *ptr = allocateAligned(size, alignment);
        errno = callerErrno;
        if (ptr == nullptr) {
            return ENOMEM;
        }
        *memptr = ptr;
        return 0;
    }
    return realPosixMemalign(memptr, alignment, size);
}

void *aligned_alloc(size_t alignment, size_t size) noexcept {
    resolveSymbols();
    if (isValidAlignment(alignment) && (shouldAlign(size) || (realAlignedAlloc == nullptr))) {
        return allocateAligned(size, alignment);
    }
    if (realAlignedAlloc == nullptr) {
        errno = EINVAL;
        return nullptr;
    }
    return realAlignedAlloc(alignment, size);
}

size_t malloc_usable_size(void *ptr) noexcept {
    resolveSymbols();
    if (ptr == nullptr) {
        return 0u;
    }

    size_t length = 0u;
    if (findMapping(ptr, length)) {
        return length;
    }
    return (realMallocUsableSize != nullptr) ? realMallocUsableSize(ptr) : 0u;
}
}
// NOLINTEND(readability-identifier-naming)

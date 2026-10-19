<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Shared System USM

* [Introduction](#introduction)
* [Enabling Shared System USM](#enabling-shared-system-usm)
* [Address space requirement](#address-space-requirement)
* [5-level paging (la57)](#5-level-paging-la57)
* [Diagnosing why Shared System USM is disabled](#diagnosing-why-shared-system-usm-is-disabled)
* [Allocation alignment](#allocation-alignment)
* [2 MB alignment preload helper](#2-mb-alignment-preload-helper)

# Introduction

Shared System USM lets a GPU kernel access ordinary system memory directly
through the same pointer the CPU uses, including memory obtained from `malloc`
or `new`. There is no separate USM allocation and no explicit copy: when the GPU
touches a system pointer that is not yet resident, a recoverable page fault is
serviced and execution continues. This relies on two pieces working together:

* the Kernel Mode Driver (KMD) exposing CPU address mirroring and recoverable
  page faults, and
* the CPU and GPU sharing a compatible virtual address layout.

# Enabling Shared System USM

Support is reported per device and exposed to applications through the shared
system memory capabilities (read/write, atomic, concurrent and concurrent
atomic access). On platforms where it is not enabled by default, it can be
requested for evaluation with:

```
NEOReadDebugKeys=1 EnableSharedSystemUsmSupport=1 <application>
```

The driver only advertises the capability when both the KMD supports it and the
address space requirement below is met.

# Address space requirement

Because the GPU dereferences CPU pointers directly, the GPU virtual address
range must be at least as large as the CPU virtual address range. If the CPU can
form addresses that the GPU cannot represent, the feature cannot be offered
safely and the driver disables it.

# 5-level paging (la57)

Traditional x86-64 systems use 4-level paging with a 48-bit virtual address
space. 5-level paging extends this to 57 bits and is advertised by the `la57`
CPU flag. When 5-level paging is active, the CPU virtual address range (57-bit)
exceeds the virtual address range of GPUs whose address space is 48-bit, so the
address space requirement above is not met and Shared System USM is disabled.

To use Shared System USM on such a system, disable 5-level paging so the CPU
falls back to 4-level (48-bit) paging by adding `no5lvl` to the kernel command
line:

1. Edit the bootloader configuration, for example `/etc/default/grub`, and
   append `no5lvl` to `GRUB_CMDLINE_LINUX` (or `GRUB_CMDLINE_LINUX_DEFAULT`),
   keeping a space between arguments.
2. Regenerate the bootloader configuration (for example `sudo update-grub`, or
   `sudo grub2-mkconfig -o /boot/grub2/grub.cfg` on RPM-based distributions).
3. Reboot.

After the reboot the CPU virtual address range matches the GPU and Shared System
USM is advertised again.

# Diagnosing why Shared System USM is disabled

Run the application with debug messages enabled:

```
NEOReadDebugKeys=1 EnableSharedSystemUsmSupport=1 PrintDebugMessages=1 <application>
```

The driver prints the reason it declined the feature. Two common messages are:

* `Shared System USM NOT allowed: KMD does not support` - the kernel driver does
  not expose CPU address mirroring / recoverable page faults.
* `Shared System USM NOT allowed: CPU address range > GPU address range due to
  5-level paging (la57). Add 'no5lvl' to the kernel command line ...` - the CPU
  virtual address space is wider than the GPU can address because of 5-level
  paging; follow the [5-level paging](#5-level-paging-la57) steps.

# Allocation alignment

The granularity at which a system allocation migrates to device memory follows
the alignment and the size of the host virtual address range it lives in. A
buffer whose start address is not 2 MB aligned is serviced partly at smaller
granularity, which multiplies the number of page faults for the same amount of
data and loses the larger page sizes the GPU prefers.

Ordinary host allocators do not align for this. `malloc` and `operator new`
guarantee only fundamental alignment, and a container such as `std::vector`
inherits that through `std::allocator`, so a large buffer usually starts at an
arbitrary page boundary. Transparent huge pages do not fix it
either: when `/sys/kernel/mm/transparent_hugepage/enabled` reports `madvise`,
an allocation is backed by huge pages only if it asks with
`madvise(MADV_HUGEPAGE)`.

An application that owns its allocation sites should therefore allocate the
buffers it passes to the GPU with a 2 MB alignment and hint them, for example:

```c
size_t alignment = 2 * 1024 * 1024;
size_t size = (bytes + alignment - 1) & ~(alignment - 1);
void *ptr = NULL;
if (posix_memalign(&ptr, alignment, size) == 0) {
    madvise(ptr, size, MADV_HUGEPAGE);
}
```

Do not use `hugetlbfs` for this. Explicitly reserved huge pages
(`/proc/sys/vm/nr_hugepages`, `MAP_HUGETLB`, or
`GLIBC_TUNABLES=glibc.malloc.hugetlb=2`) produce aligned memory, but the kernel
does not migrate a `hugetlbfs` backed mapping to device memory at all, so such
a buffer is faulted from system memory for the whole lifetime of every kernel
that reads it.

# 2 MB alignment preload helper

When the allocation sites cannot be changed - a closed source application, or a
library that allocates on the caller's behalf - `shared/align2m_preload` builds
a small `LD_PRELOAD` library that applies the same alignment from the outside.
It interposes the C allocation entry points and serves every request at or
above a threshold from a private anonymous mapping that is 2 MB aligned, 2 MB
sized and hinted with `MADV_HUGEPAGE`. Smaller requests are passed through to
the process allocator unchanged.

The helper is a development and evaluation tool. It ships as source only: the
driver build does not compile it and it is not part of the driver package.

## Building

The directory is a standalone CMake project:

```
cmake -S shared/align2m_preload -B <build directory> -DCMAKE_BUILD_TYPE=Release
cmake --build <build directory>
```

The source is self contained, so it can also be compiled directly:

```
g++ -shared -fPIC -O2 -Wall -Wextra -std=c++17 -D_GNU_SOURCE \
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free \
    -o libalign2m_preload.so shared/align2m_preload/source/align2m_preload.cpp \
    -ldl -pthread
```

## Using

Preload the library for the process under test:

```
LD_PRELOAD=<path>/libalign2m_preload.so <application>
```

The threshold above which a request is redirected defaults to 2 MB, which is
the smallest request that fills a 2 MB page. `ALIGN2M_THRESHOLD` overrides it
with a size in bytes, decimal or `0x` hexadecimal. `0` turns the redirection
off, which leaves the library loaded and every allocation on the process
allocator:

```
ALIGN2M_THRESHOLD=8388608 LD_PRELOAD=<path>/libalign2m_preload.so <application>
ALIGN2M_THRESHOLD=0 LD_PRELOAD=<path>/libalign2m_preload.so <application>
```

Each redirected request is rounded up to a whole number of 2 MB pages and gets
its own mapping, so nothing is packed together and resident memory grows in
2 MB steps per buffer. A 2100 KB buffer that is fully written therefore occupies
4 MB. A threshold below 2 MB also redirects buffers smaller than 2 MB, and each
of them then takes a whole 2 MB of address space and up to 2 MB of resident
memory once written. An application that allocates many such buffers consumes
noticeably more memory than it asks for, which is why the default is 2 MB.

## Confirming it took effect

The redirected buffers are visible as private anonymous mappings whose start and
end are both 2 MB aligned, so their size is a multiple of 2 MB. 2 MB alignment
means the low 21 bits are clear, which is five trailing zero hexadecimal digits
preceded by an even digit:

```
grep -E "^[0-9a-f]*[02468ace]00000-[0-9a-f]*[02468ace]00000 rw-p 00000000 00:00 0" /proc/<pid>/maps
```

The trailing fields pin the mapping to an anonymous one: offset 0, device 00:00
and inode 0. Without them a file-backed private mapping that happens to have
2 MB aligned endpoints would be reported as evidence.

The page fault granularity the GPU actually sees is exposed per GT. The counters
are cumulative, so read them before and after the run. The render device node is
not always `dri/0`, which on a system with a display is the display device:

```
sudo grep -E "svm_(4K|64K|2M)_pagefault_count" /sys/kernel/debug/dri/*/gt0/stats
```

With the allocations aligned, the 4K and 64K counters stay unchanged across a
run and the whole delta appears on the 2M counter.

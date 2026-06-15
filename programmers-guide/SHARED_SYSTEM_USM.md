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

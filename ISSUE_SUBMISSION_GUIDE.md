<!---

Copyright (C) 2025-2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# GitHub Issue Submission Guide

## Purpose
This guide explains how to submit issues using the structured GitHub Issue Templates available in this repository.

---

## Before You Submit

- **Search existing issues** in the [Issues tab](../../issues) to avoid duplicates.
- **Use the latest driver version** before reporting a bug - check [releases](https://github.com/intel/compute-runtime/releases).
- **Check the [FAQ](https://github.com/intel/compute-runtime/blob/master/FAQ.md) and [programmers-guide](https://github.com/intel/compute-runtime/tree/master/programmers-guide)** for questions that may already be answered.

---

## Choosing the Right Template

When opening a new issue, GitHub will prompt you to select a template. Blank issues are not accepted. Choose the template that best matches your situation:

| Template | Use when | Covers |
|---|---|---|
| **[[Bug Report] Linux GPU Driver Issue](https://github.com/intel/compute-runtime/issues/new?template=1-linux-gpu-driver-issue.yml)** | Driver bugs, crashes, hangs, or incorrect behavior on Linux | System info, installed packages, kernel version, reproduction steps, regression details, API/strace/dmesg logs, backtraces, reproducer code |
| **[[Bug Report] Windows GPU Driver Issue](https://github.com/intel/compute-runtime/issues/new?template=2-windows-gpu-driver-issue.yml)** | Driver bugs, TDR errors, crashes, or incorrect behavior on Windows | GPU and Windows build info, reproduction steps, regression details, API logs, Event Viewer logs, crash dumps, reproducer code |
| **[[Feature Request]](https://github.com/intel/compute-runtime/issues/new?template=3-feature-request.yml)** | Proposing a new feature or enhancement | Problem statement, proposed solution, expected benefits, use case scenarios, API design examples, alternatives, priority |
| **[[Question]](https://github.com/intel/compute-runtime/issues/new?template=4-question.yml)**| Questions about the project, API usage, installation, or behavior | Question category, detailed question, additional context |

---

## Submission Guidelines

- **Follow the in-template instructions** - each field includes collapsible guidance with commands to run and example outputs.
- **Paste excerpts, attach full files** - for logs (dmesg, strace, API traces), paste a relevant excerpt in the field and attach the complete log file for thorough analysis.
- **Complete the pre-submission checklist** at the top of each template before submitting.
- **Use a clear, descriptive title** - maintainers will prepend an internal tracking number (e.g., `[GSD-1234]`) after creation.
- **Be professional** - use respectful language and focus on technical details.

---

## After You Submit

### Labels
Issues are automatically labeled based on the template used (`Type:`, `OS:`). Maintainers will add `Component:` labels during triage.

You may also see a **status label** applied as your issue progresses:

| Label | What it means |
|---|---|
| **Status: Needs Feedback** | The team needs more information from you - please check for a comment |
| **Status: In Progress** | Actively being investigated |
| **Status: Transferred** | Root cause identified in another component; issue moved to the relevant team |
| **Status: Backlog** | Confirmed valid; will be investigated and fixed when resources are available |
| **Status: Merged** | Fix is on the master branch, pending an official release |
| **Status: Marked as Stale** | No activity for 30+ days; the issue may be closed if there is no response |

---

By using the structured templates, you can help maintainers address issues effectively and collaboratively improve the project.
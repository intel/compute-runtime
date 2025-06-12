<!---

Copyright (C) 2025 Intel Corporation

SPDX-License-Identifier: MIT

-->

# GitHub Issue Submission Guide

## Purpose
This guide provides best practices for submitting issues to ensure effective communication and resolution.

---

## Steps to Submit an Issue

### 1. Search Existing Issues
Before creating a new issue, check the repository's **Issues** tab to see if the problem has already been reported or resolved.

### 2. Write a Clear Title
Use a concise and descriptive title that summarizes the issue.

### 3. Detail the Issue
Include the following information in the issue description:
- **What happened?** Clearly explain the problem.
- **Steps to reproduce:** Provide steps to replicate the issue.
- **Expected behavior:** Describe the expected outcome.
- **Actual behavior:** Explain the actual outcome.

### 4. Attach Supporting Information
Include any relevant details to assist in resolving the issue:
- Error messages or logs (e.g., `dmesg`, `strace` for Linux)
- Code snippets (if applicable)
- Binary reproducer (with reproduction steps)
- Problem classification (e.g., stability/crash, functional, performance)
- Operating System details (e.g., Linux distro, kernel version)
- Environment specifics (e.g., GPU driver version, oneAPI stack, hardware)
- Reproduction rate (e.g., 100% reproduction, sporadic)
- Regression details (e.g., youngest known working driver, oldest known failing driver versions)
- Screenshots or videos

### 5. Additional Debugging Tools
Optionally, you may include artifacts/logs generated using the following tools:
- **Unitrace**: [Intel PTI GPU unitrace](https://github.com/intel/pti-gpu/tree/master/tools/unitrace)
- **Shader dumps**: [Intel Graphics Compiler shader dumps](https://github.com/intel/intel-graphics-compiler/blob/master/documentation/shader_dumps_instruction.md)
- **CLIntercept**: [OpenCL Intercept Layer logs](https://github.com/intel/opencl-intercept-layer) (e.g., api call logs)

### 6. Maintain Professionalism
Use respectful and professional language when describing the issue. Avoid blaming or criticizing contributors.

---

## Example Issue Template

```markdown
### Issue Title: [Brief description of the issue]

#### Description
[Detailed explanation of the issue]

#### Steps to Reproduce
1. [Step 1]
2. [Step 2]
3. [Step 3]

#### Expected Behavior
[What you expected to happen]

#### Actual Behavior
[What actually happened]

#### Additional Information
- Logs: [Attach if applicable]
- Environment: [OS, GPU driver version, etc.]
...
```

---

By following this guide, you can help maintainers address issues effectively and collaboratively improve the project.
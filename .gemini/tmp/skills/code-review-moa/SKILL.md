---
name: code-review-moa
description: Perform a multi-perspective code review using a Mixture of Agents approach (Developer, Tester, and Security Analyst). Use when you want to review uncommitted changes, specific files, or recent commits for quality, reliability, and security.
---

# Code Review (Mixture of Agents)

This skill enables you to perform a comprehensive code review by simulating three distinct expert personas.

## Workflow

1. **Gather Changes:** Identify the code to be reviewed (e.g., `git diff`, `git diff HEAD~1`, or specific files).
2. **Execute Multi-Perspective Review:** Analyze the changes sequentially from three perspectives:
    - **Developer:** Read [developer.md](references/developer.md) for focus areas.
    - **Tester:** Read [tester.md](references/tester.md) for focus areas.
    - **Security Analyst:** Read [security.md](references/security.md) for focus areas.
3. **Consolidate Findings:** Synthesize the feedback into a single, cohesive report.
4. **Actionable Suggestions:** Provide specific, line-by-line recommendations for improvement.

## Instructions for the Agent

- **Be Critical but Constructive:** Aim to improve the code, not just find faults.
- **Reference Specific Lines:** Use line numbers or code snippets to make feedback actionable.
- **Prioritize Issues:** Distinguish between critical bugs, security risks, and minor style "nits".
- **Respect Local Conventions:** Always check `GEMINI.md` or local style guides before suggesting changes that might conflict with project standards.

## Example Triggers

- "Review my current changes using MoA."
- "Give me a security and quality review of the last commit."
- "Perform a Mixture of Agents review on `src/main.cc`."

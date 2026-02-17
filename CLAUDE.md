# Claude Code Instructions

## Commit Policy

Commit automatically after completing each substantial change -- a working feature, bug fix, or meaningful refactor. Don't wait for the user to ask. Write clear commit messages in imperative mood. Do not include a Co-Authored-By line.

## README as Source of Truth

The README documents match timing, state machine flow, pin mapping, light states, and wiring. After making any code change, re-read the README and verify it still matches the code. If a code change conflicts with what the README says (new states, changed timing, different pins, new light behaviors, etc.), update the README in the same commit.

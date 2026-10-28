# Commit messages and PR descriptions

The format of commit messages and pull request descriptions in this repo. Claude writes commit messages itself in `/commit-issue`; the `pr-description` agent (`.claude/agents/pr-description.md`) writes PR descriptions for `/finish-issue`. Both follow this file. The workflow around them is in `docs/workflow.md`.

## Commit message

```
#<n> <issue title>: <topic>

* <change>
* <change>
```

- Line 1: `#<n> <issue title>: <topic>` - the exact GitHub title (`gh issue view <n> -R tzijnge/SolderingStation --json title --jq .title`), then two to five lowercase words naming what this particular commit covers, e.g. `#1 Hard over-temperature cutoff: fault screen`. Always include the topic: an issue gets several commits over time, and without it they'd all share the same first line, so `git log --oneline` couldn't tell them apart.
- Keep line 1 at most 72 characters, suffix included. If it doesn't fit, shorten the topic first, then the title.
- Line 2: always blank. (Git treats everything up to the first blank line as the subject, so without it tools show the whole message as one line.)
- Then one bullet per feature or modification, each on its own line starting with `* `.
- Never write a GitHub closing keyword (close/closes/closed, fix/fixes/fixed, resolve/resolves/resolved) directly before an issue reference (`#<n>`) anywhere in a commit message - only a PR description ends with one. Issues are closed through the pull request on GitHub, and GitHub also acts on such keywords in the commits of a merged PR. A keyword not followed by `#<n>` ("Fix display flicker") is fine.
- The only trailer is the `Co-Authored-By` line, added when committing.

Describe what is staged, from the actual diff (`git diff --cached`) - never from file names or from memory of the conversation alone. For large generated files (e.g. `src/images/*.h`, marked "GENERATED"), the header comment is enough.

## Writing the bullets

- Imperative mood, describing the effect: "Add fault screen with warning sign", not "Added FaultScreen.h" or "This commit adds...".
- One bullet per user-visible feature or meaningful change, not per file. Group related file changes into one bullet. Aim for roughly 3-10 bullets; never pad.
- Add a short reason in parentheses when the why isn't obvious from the what, e.g. "Lower max setpoint to 430C (margin below the 450C cutoff)".
- Order: the issue's own feature(s) first, then supporting changes, then refactoring/tooling/docs.
- Explicitly mark, as their own bullets:
  - changes outside the issue's scope: prefix with "Also: ";
  - behaviour changes a user would notice (button mapping, limits, screens);
  - anything unfinished, untested on hardware or test-only, e.g. a constant left at a test value, a `TEMPORARY` marker, a commented-out block: prefix with "NOTE: ".
- Name things the way the code and the README do (class/file names in backticks only when it helps a developer find them).
- No hard line breaks: each bullet is a single line, however long - messages are only read in viewers that wrap lines themselves.
- Plain ASCII where reasonable (`C`/`deg` rather than `°`) so the message reads well in every terminal.

## PR description

The title and description of the pull request that brings an issue's branch into `main`. With squash merging they become the issue's single commit on `main`, so they summarize the whole issue in the same structure as the commits. Input is all the issue's commits (`git log main..<branch> --reverse --format=%B`), the overall change (`git diff main...<branch> --stat`) and the actual diff where something needs checking.

```
#<n> <issue title>

* <change>
* <change>

Closes #<n>
```

- Line 1: `#<n> <issue title>`, exactly the GitHub title, NO topic suffix - the PR covers the whole issue.
- The bullets repeat the bullets of the commits - keep their wording where possible - with irrelevant items removed. Irrelevant:
  - fixes for QA or review comments, build fixes, typo fixes;
  - added or updated tests;
  - anything reverted, superseded or made redundant by a later commit on the same branch - describe only the end result (a banner later replaced by a full screen: only the screen; a value changed twice: only its final value; a file added and removed again: nothing);
  - notes that no longer hold at the end of the branch: "not yet tested" when it was tested later, "intermediate state not built", references to other commits of the branch;
  - pure housekeeping: formatting, line endings, whitespace, comment rewording.
- Keep "Also:" items (real changes on main, outside the issue's scope) and "NOTE:" items that are still true at the end of the branch, e.g. code still marked TEMPORARY - check the final code.
- Merge duplicates across commits into one bullet. Order: the issue's own features first, then supporting changes, then "Also:", then "NOTE:".
- A blank line, then `Closes #<n>` as the very last line - the only place a closing keyword is allowed, because it closes the issue when the PR is merged. No trailers.
- All bullet rules above apply.

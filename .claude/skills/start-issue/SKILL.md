---
name: start-issue
description: Start work on a GitHub issue of this repo - check the previous issue is merged, branch "#<n>-<issue-title-with-dashes>" from an up-to-date main, move the issue to In Progress on the project board, and propose a short implementation plan for the user's OK before writing code. Use when the user asks to start/pick up/work on an issue (e.g. "/start-issue 2", "let's do #2").
argument-hint: <issue number>
---

# Start an issue

Read `docs/workflow.md` first: it holds the conventions (branch naming, board, merge flow) this skill follows. Prefix `gh` commands with `export PATH="$PATH:/c/Program Files/GitHub CLI";`.

## 1. The issue

- `gh issue view <n> -R tzijnge/SolderingStation --json number,title,body,state,labels`.
- Doesn't exist: stop. Closed: ask whether to reopen it or pick another issue.
- Read the body fully - it's the starting point for the plan in step 5.

## 2. Clean starting point

- `git status --short`: uncommitted changes must not silently travel to the new branch. If there are any, stop and ask: commit them first (`/commit-issue` for their issue), stash them, or discard them.
- Unmerged previous work: list local issue branches (`git branch --list '#*'`) and open PRs (`gh pr list -R tzijnge/SolderingStation --state open`). For each branch, check whether its PR is merged (`gh pr list --head '<branch>' --state merged`). If there's unmerged work, tell the user and ask: wait, merge it first, or - only if this issue depends on it - branch off that branch instead of `main` (then this branch needs a rebase once the other is merged; say so).

## 3. The branch

- `git switch main` and `git pull --ff-only`. If the fast-forward fails, stop and ask - main has diverged.
- Branch name per `docs/workflow.md`; `git switch -c '<branch>'`. If it already exists (locally or on origin), switch to it instead and say so - work on this issue has been started before.

## 4. The board

Move the issue to *In Progress* on project 2 (owner tzijnge):
- Item id: from `gh project item-list 2 --owner tzijnge --limit 200 --format json`, the item whose `content.number` is `<n>`. If the issue isn't on the board yet, add it: `gh project item-add 2 --owner tzijnge --url <issue url> --format json`.
- Project id: `gh project view 2 --owner tzijnge --format json` → `id`. Status field and option: `gh project field-list 2 --owner tzijnge --format json` → the field named `Status` and its option named `In Progress` (look them up by name; ids can change).
- `gh project item-edit --id <item> --project-id <project> --field-id <field> --single-select-option-id <option>`.

## 5. The plan

Before writing any code, re-read the issue against the current code (it may have been written before recent changes) and present a comprehensive plan:
- what changes, by file or component, in a few bullets;
- anything in the issue that's outdated or no longer fits the code;
- open decisions, as concrete questions with a recommendation each;
- what will need testing on the device.

Wait for the user's OK (or changes) before writing code. Then implement, following the project's standing rules (memory notes: C++ style, state-diagram updates, manual builds).

---
name: commit-issue
description: Commit the current work for a GitHub issue of this repo onto its issue branch - creates/switches to the branch "#<n>-<issue-title-with-dashes>", checks for leftovers and line-ending noise, stages the work as one self-contained commit (split only when the user asks, and only into commits that each build and pass tests), writes the message per docs/commit-messages.md, commits after the user's review and pushes each commit; opens a draft PR for the branch after its first commit. Use when the user asks to commit work for an issue (e.g. "/commit-issue 1", "commit this for #3").
argument-hint: <issue number>
---

# Commit work for an issue

Commits the uncommitted work in this repo (tzijnge/SolderingStation) for one GitHub issue onto that issue's branch. You run this in the main session because it needs the conversation's context: what was tested, which values are deliberate, what belongs together. You also write the commit messages yourself, in the format of `docs/commit-messages.md`.

Read `docs/workflow.md` and `docs/commit-messages.md` first: they hold the conventions (branch naming, board, commit message format) this skill follows. Prefix `gh` commands with `export PATH="$PATH:/c/Program Files/GitHub CLI";`. Normally the branch already exists from `/start-issue`; step 2 covers the cases where it doesn't.

## 1. Identify the issue

- The issue number is the argument. Without one, take it from the current branch name (`#<n>-...`). Still none: ask the user.
- `gh issue view <n> -R tzijnge/SolderingStation --json number,title,state`. If the issue doesn't exist, stop. If it's closed, ask before continuing.

## 2. Get onto the issue branch

Branch name per `docs/workflow.md` (always quoted in the shell).

- Already on that branch: continue.
- On `main` (or another branch without commits of its own) with the work uncommitted: `/start-issue` was skipped. `git switch -c '<branch>'` (uncommitted changes come along), and move the issue to *In Progress* on the board as in `/start-issue` step 4.
- The branch exists but isn't checked out: `git switch '<branch>'` if git allows it with the current changes; otherwise stop and ask.
- On a different issue's branch: stop and ask - the work may belong to that issue.

## 3. Check before staging

Fix or ask about each of these before proposing commits:
- **Leftover test values**: from the conversation, any constant or code changed only for testing (e.g. a limit lowered to trigger a fault, a temporary test event). Ask the user whether to restore it; never commit a test value silently.
- **Line endings**: `git ls-files --eol` - any file with `i/lf` and `w/crlf` was rewritten with CRLF by an edit; convert it back with `sed -i 's/\r$//'`. Same for new untracked text files containing `\r`. Files already `i/crlf` in the repo stay as they are.
- **Stray files**: untracked files that don't belong in the repo (scratch output, local config). Ask before including anything unclear.
- **Build/test status**: note from the conversation what the user built and tested on hardware and what not - the messages need it (step 6).

## 4. One commit, unless the user asks for a split

By default all uncommitted work becomes one commit: every commit must be self-contained, i.e. CI must be able to build and test it successfully, and splitting source changes into separate subjects rarely gives commits that each build. Don't propose a split on your own; the user asks for one when they find the work too big. Give the commit a topic (two to five words) covering the work as a whole, and go to step 5.

When the user asks for a split: propose commits that each build and pass the tests on their own, even if that makes the topics less clean (e.g. a refactoring together with the code that needs it). Order them so each commit only depends on earlier ones. Show a table (number, topic, files - noting files that span commits) and wait for approval before staging anything.

## 5. Stage the commit

- Whole files: `git add <paths>` (normally all changes: `git add -A`, after the checks of step 3).
- Only for a requested split, a file that spans commits: build the intermediate version for the earlier commit (e.g. the committed version plus only that topic's hunks) in the scratchpad, then put it in the index without touching the working tree:
  `git update-index --cacheinfo 100644,$(git hash-object -w <file>),<path>`
  (interactive `git add -p` is not available). Check the intermediate version: the final version must equal it plus the later topics' changes.
- Before each commit, check `git diff --cached --stat` (and the diff for intermediate versions) contains exactly the planned changes.

## 6. Message per commit

Write the message yourself, per `docs/commit-messages.md`:
- Read the staged diff (`git diff --cached`) and describe exactly that - not what the conversation intended. The conversation adds what the diff can't show: what was tested on hardware and what not (untested parts get a `NOTE:` bullet), which values are deliberate, why.
- Subject: the issue's exact GitHub title (step 1) and the commit's topic (step 4).
- Check it against the rules before showing it: subject at most 72 characters, blank line 2, one line per bullet, no closing keyword before `#<n>`.

## 7. User review - always, for every commit

Never commit without the user's explicit OK for that commit - no exceptions, also not for small tooling or docs commits. With the commit staged and its message checked:
- show the user `git diff --cached --stat` and the proposed message (in a code block) - not the full diff;
- tell them the commit is staged so they can view its diff in their own diff viewer (the index holds exactly this commit, also for intermediate versions from step 5), and ask for their OK on the changes and the message, then wait. Viewing the diff is their choice; the OK is not optional;
- if they want changes: make them, re-stage, regenerate or adjust the message if needed, and ask again.
Only an OK given after seeing this commit's staged changes counts. When several commits are planned, review them one at a time.

## 8. Commit

After the OK: write the message to a file in the scratchpad, append a blank line and the `Co-Authored-By` trailer from the session's attribution instructions, and `git commit -F <file>`. Never use `--no-verify`, `--amend` (every commit gets pushed), or force options.

## 9. Push

Right after each commit, before staging the next one: `git push -u origin '<branch>'`. If the push is rejected (origin has commits this branch doesn't), stop and ask - never force-push.

## 10. Draft PR

After the first push of a branch, check for a PR: `gh pr list -R tzijnge/SolderingStation --head '<branch>' --state open --json number,url,isDraft`. If there is none, open one as a draft, without reviewers, so CI runs on every following push:
- title `#<n> <issue title>` (per `docs/workflow.md`);
- a placeholder description in the scratchpad - the real one is written by `/finish-issue`:

  ```
  Work in progress - the description follows when the issue is finished.

  Closes #<n>
  ```
- `gh pr create -R tzijnge/SolderingStation --draft --base main --head '<branch>' --title '<title>' --body-file <file>`.

If an open PR exists, leave it alone; if it isn't a draft any more, mention that (the user may have marked it ready too early).

## 11. Finish

- `git status --short` must be clean (or only show what the user chose to leave out), `git log --oneline main..HEAD` lists the new commits, and `git status -sb` shows the branch in sync with origin.
- The PR closes the issue - so no closing keywords (`Closes #n`) in commits.
- Report the commits (hash, subject, topic), the PR link, and anything not verified, e.g. intermediate commits of a split not built on their own (Claude doesn't run `pio`; until CI exists, only the user's build of the final state is verified).

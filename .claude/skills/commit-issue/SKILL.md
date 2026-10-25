---
name: commit-issue
description: Commit the current work for a GitHub issue of this repo onto its issue branch - creates/switches to the branch "#<n>-<issue-title-with-dashes>", checks for leftovers and line-ending noise, proposes how to divide the work into topic commits, stages them, gets each message from the commit-message agent and commits. Never pushes. Use when the user asks to commit work for an issue (e.g. "/commit-issue 1", "commit this for #3").
argument-hint: <issue number>
---

# Commit work for an issue

Commits the uncommitted work in this repo (tzijnge/SolderingStation) for one GitHub issue onto that issue's branch. You run this in the main session because it needs the conversation's context: what was tested, which values are deliberate, what belongs together. Commit messages come from the `commit-message` agent (`.claude/agents/commit-message.md`); this skill owns everything around them.

`gh` may need `export PATH="$PATH:/c/Program Files/GitHub CLI"` in Git Bash.

## 1. Identify the issue

- The issue number is the argument. Without one, take it from the current branch name (`#<n>-...`). Still none: ask the user.
- `gh issue view <n> -R tzijnge/SolderingStation --json number,title,state`. If the issue doesn't exist, stop. If it's closed, ask before continuing.

## 2. Get onto the issue branch

Branch name: `#<n>-` followed by the issue title in lowercase, with every run of characters other than `a-z` and `0-9` replaced by a single `-`, and no leading or trailing `-`. Examples:
- #1 "Hard over-temperature cutoff" -> `#1-hard-over-temperature-cutoff`
- #10 "Selectable temperature step (1/5/10)" -> `#10-selectable-temperature-step-1-5-10`

Always quote it in the shell: `#` starts a comment.

- Already on that branch: continue.
- On `main` (or another branch without commits of its own) with the work uncommitted: `git switch -c '<branch>'`; uncommitted changes come along.
- The branch exists but isn't checked out: `git switch '<branch>'` if git allows it with the current changes; otherwise stop and ask.
- On a different issue's branch: stop and ask - the work may belong to that issue.

## 3. Check before staging

Fix or ask about each of these before proposing commits:
- **Leftover test values**: from the conversation, any constant or code changed only for testing (e.g. a limit lowered to trigger a fault, a temporary test event). Ask the user whether to restore it; never commit a test value silently.
- **Line endings**: `git ls-files --eol` - any file with `i/lf` and `w/crlf` was rewritten with CRLF by an edit; convert it back with `sed -i 's/\r$//'`. Same for new untracked text files containing `\r`. Files already `i/crlf` in the repo stay as they are.
- **Stray files**: untracked files that don't belong in the repo (scratch output, local config). Ask before including anything unclear.
- **Build/test status**: note from the conversation what the user built and tested on hardware and what not - the messages need it (step 6).

## 4. Propose the commits

Group the changes into topic commits - the issue's own feature, plus separate commits for groundwork it needs (toolchain, refactoring), tooling, docs or notes that stand on their own. Order them so each commit only depends on earlier ones; ideally every commit builds. One commit is fine when the work is one topic.

Show the user a table (number, topic, files - noting files that span topics) and wait for approval before staging anything.

## 5. Stage each commit

- Whole files: `git add <paths>`.
- A file that spans topics: build the intermediate version for the earlier commit (e.g. the committed version plus only that topic's hunks) in the scratchpad, then put it in the index without touching the working tree:
  `git update-index --cacheinfo 100644,$(git hash-object -w <file>),<path>`
  (interactive `git add -p` is not available). Check the intermediate version: the final version must equal it plus the later topics' changes.
- Before each commit, check `git diff --cached --stat` (and the diff for intermediate versions) contains exactly the planned changes.

## 6. Message per commit

Run the `commit-message` agent in the foreground for each staged commit. Its prompt must include:
- the issue number;
- the commit's topic, e.g. "topic: image tooling" - the agent ends the subject with it;
- "describe only the staged changes; skip the suggestion on dividing them";
- context not visible in the diff: what was tested on hardware and what not, which values are deliberate, what earlier commits already covered.

Check the returned message against the code before using it - fix factual errors yourself (e.g. a wrong value), don't pass them on.

## 7. Commit

Write the message to a file in the scratchpad, append a blank line and the `Co-Authored-By` trailer from the session's attribution instructions, and `git commit -F <file>`. Never use `--no-verify`, `--amend` on pushed commits, or force options.

## 8. Finish

- `git status --short` must be clean (or only show what the user chose to leave out), and `git log --oneline main..HEAD` lists the new commits.
- Never push unless the user asks. The user opens the PR on GitHub's PR page and closes the issue from there - so no closing keywords (`Closes #n`) in commits.
- Report the commits (hash, subject, topic) and anything not verified, e.g. intermediate commits not built on their own.

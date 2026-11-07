---
name: finish-issue
description: Finish a GitHub issue of this repo - confirm hardware testing, make sure everything is committed and pushed, and have the pr-description agent write the final PR title and description for the draft PR (opened by /commit-issue, created here if missing; refreshed when run again after review commits); after the user has merged it, switch to the updated main, delete the local branch and verify the issue is closed and Done. Use when the user says an issue is done/ready for a PR (e.g. "/finish-issue 1"), that review comments were processed, or that its PR is merged.
argument-hint: <issue number>
---

# Finish an issue

Read `docs/workflow.md` first: it holds the conventions (PR format, squash merge, board) this skill follows. Prefix `gh` commands with `export PATH="$PATH:/c/Program Files/GitHub CLI";`.

Determine the issue number (argument, or the current branch name) and its exact title (`gh issue view <n> -R tzijnge/SolderingStation --json title,state`). Then check where the issue stands - `gh pr list -R tzijnge/SolderingStation --head '<branch>' --state all --json number,state,url` - and continue with part A (no PR yet, or still open) or part B (PR merged).

## A. Ready for a PR

This part also runs again after review rounds. If the PR already has its real description (its body doesn't start with the placeholder "Work in progress"), it's a **refresh**: further commits, typically from review comments, may have changed what the description should say, and the description is the future squash commit on `main`. In a refresh, step 1 only concerns the code changed since the tests the user last confirmed, step 5 replaces the existing description, the PR's state (draft or ready for review) is never changed, and step 6 reports what changed compared with the old description.

1. **Hardware tests confirmed.** From the conversation and the commits (look for `NOTE:` bullets about untested parts: `git log main..HEAD --format=%B`), list what must be tested on the device for this issue. Ask the user to confirm each item is tested, unless they already did so in the conversation for the current state of the code. Don't create the PR until they confirm.
2. **Everything committed.** `git status --short` must be clean. If not, run `/commit-issue` first (ask if unrelated changes are lying around).
3. **Pushed.** `/commit-issue` pushes every commit, so the branch should be in sync with origin (`git status -sb`). If it isn't, push it - `git push -u origin '<branch>'` - and say so.
4. **Draft the PR**: title and description in the format of `docs/workflow.md` (with the repo's squash setting they also become the commit on main). Run the `pr-description` agent (`.claude/agents/pr-description.md`) in the foreground for issue `<n>` on branch `<branch>`; it follows `docs/commit-messages.md`. Give it the context it can't see: what was tested on the device (step 1), which values are final, which items were reverted or superseded by later commits if you know.
   Check its result against the final code and the commit list before passing it on: nothing relevant dropped, nothing irrelevant kept (list in `docs/commit-messages.md`), final values correct. Fix it yourself where needed.
5. **Update the draft PR**: write the description to a scratchpad file. Normally `/commit-issue` already opened a draft PR with a placeholder description: replace it with `gh pr edit <pr> -R tzijnge/SolderingStation --title '<title>' --body-file <file>`. If there is no open PR, create it: `gh pr create -R tzijnge/SolderingStation --draft --base main --head '<branch>' --title '<title>' --body-file <file>`. Then check `gh pr view <pr> --json isDraft,closingIssuesReferences` - it must list issue `<n>` (the link can take a few seconds to appear), and be a draft, except in a refresh, where it stays in whatever state it was.
6. Report the PR link, the description you used, and the list of dropped bullets (so the user can disagree - edit the PR if they do); in a refresh also which bullets were added, dropped or reworded compared with the old description, or that it's unchanged. Remind the user that a draft PR must be marked **Ready for review** before it can be merged (skip this if it already is), that they merge it with **Squash and merge**, and to tell you once it's merged (part B).

## B. After the merge

1. **Verify the merge**: the PR's state is `MERGED` (`gh pr view <pr> -R tzijnge/SolderingStation --json state,mergedAt,mergeCommit`). If it's not, stop - nothing below may happen.
2. **Updated main**: `git switch main`, `git pull --ff-only`. Check the squash commit is there (`git log -1 --oneline`).
3. **Delete the local branch**: after a squash merge git doesn't recognise the branch as merged, so `git branch -d` refuses. Since step 1 verified the merge on GitHub, use `git branch -D '<branch>'`. Also `git fetch --prune` to drop the stale remote-tracking ref if GitHub deleted the remote branch. Don't delete the remote branch yourself.
4. **Issue and board**: the issue must be closed (`gh issue view <n> --json state,stateReason`) and its board item in status *Done* (`gh project item-list 2 --owner tzijnge --limit 200 --format json`). If either isn't, report it and offer to fix it (close the issue as completed / set Status to Done as in `/start-issue` step 4).
5. Report the result in a few lines, and suggest the next issue if the user hasn't picked one.

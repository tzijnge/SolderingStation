---
name: finish-issue
description: Finish a GitHub issue of this repo - confirm hardware testing, make sure everything is committed and pushed, and have the commit-message agent draft the squash-merge PR title and description (the final commit message on main) for the user to create on GitHub; after the user has merged it, switch to the updated main, delete the local branch and verify the issue is closed and Done. Use when the user says an issue is done/ready for a PR (e.g. "/finish-issue 1") or that its PR is merged.
argument-hint: <issue number>
---

# Finish an issue

Read `docs/workflow.md` first: it holds the conventions (PR format, squash merge, board) this skill follows. Prefix `gh` commands with `export PATH="$PATH:/c/Program Files/GitHub CLI";`.

Determine the issue number (argument, or the current branch name) and its exact title (`gh issue view <n> -R tzijnge/SolderingStation --json title,state`). Then check where the issue stands - `gh pr list -R tzijnge/SolderingStation --head '<branch>' --state all --json number,state,url` - and continue with part A (no PR yet, or still open) or part B (PR merged).

## A. Ready for a PR

1. **Hardware tests confirmed.** From the conversation and the commits (look for `NOTE:` bullets about untested parts: `git log main..HEAD --format=%B`), list what must be tested on the device for this issue. Ask the user to confirm each item is tested, unless they already did so in the conversation for the current state of the code. Don't draft the PR until they confirm.
2. **Everything committed.** `git status --short` must be clean. If not, run `/commit-issue` first (ask if unrelated changes are lying around).
3. **Pushed.** If the branch has commits not on origin (`git status -sb`), push it - `git push -u origin '<branch>'`. Pushing is part of this skill's job; say that you're doing it.
4. **Draft the PR**: its title and description become the squash commit on main (format in `docs/workflow.md`). Run the `commit-message` agent in the foreground and ask for a **squash message** for issue `<n>` on branch `<branch>`. Give it the context it can't see: what was tested on the device (step 1), which values are final, which items were reverted or superseded by later commits if you know.
   Check its result against the final code and the commit list before passing it on: nothing relevant dropped, nothing irrelevant kept (list in `docs/workflow.md`), final values correct. Fix it yourself where needed.
   Give the user the title and description, each in its own code block for easy copying, the agent's list of dropped bullets (so the user can disagree), and the compare link: `https://github.com/tzijnge/SolderingStation/compare/main...<branch with # as %23>?expand=1`. Don't create the PR yourself.
5. Remind the user to choose **Squash and merge**, and to tell you once it's merged (part B).

## B. After the merge

1. **Verify the merge**: the PR's state is `MERGED` (`gh pr view <pr> -R tzijnge/SolderingStation --json state,mergedAt,mergeCommit`). If it's not, stop - nothing below may happen.
2. **Updated main**: `git switch main`, `git pull --ff-only`. Check the squash commit is there (`git log -1 --oneline`).
3. **Delete the local branch**: after a squash merge git doesn't recognise the branch as merged, so `git branch -d` refuses. Since step 1 verified the merge on GitHub, use `git branch -D '<branch>'`. Also `git fetch --prune` to drop the stale remote-tracking ref if GitHub deleted the remote branch. Don't delete the remote branch yourself.
4. **Issue and board**: the issue must be closed (`gh issue view <n> --json state,stateReason`) and its board item in status *Done* (`gh project item-list 2 --owner tzijnge --limit 200 --format json`). If either isn't, report it and offer to fix it (close the issue as completed / set Status to Done as in `/start-issue` step 4).
5. Report the result in a few lines, and suggest the next issue if the user hasn't picked one.

# Development workflow

How work on this project goes from a GitHub issue to `main`. Each phase has a Claude Code skill (in `.claude/skills/`) that follows these rules. This document is the single source for the conventions: the skills refer to it instead of repeating them.

| Phase | Skill | What happens |
|---|---|---|
| Start | `/start-issue <n>` | Branch from an up-to-date `main`, issue to *In Progress* on the board, short plan for approval |
| Work | (normal conversation) | Implement, you build and test on the device |
| Commit | `/commit-issue <n>` | Checks, one self-contained commit with a message written by Claude, pushed after your review; the first one opens a draft PR |
| Finish | `/finish-issue <n>` | Confirm hardware tests, write the final PR title and description; after you've merged it: clean up |

An issue normally gets several commits, and work on an issue can go back and forth between *Work* and *Commit* several times before it's finished.

## Conventions

### Issues and the board

- Repository: `tzijnge/SolderingStation`. Project board: "Soldering Station", user project number 2 (owner `tzijnge`), with a *Status* field: *Todo*, *In Progress*, *Done*.
- Starting an issue moves it to *In Progress*. When its PR is merged, the issue closes and moves to *Done*.
- Issues are closed only through their PR (`Closes #<n>` in the PR description), never by a commit message.

### Branches

- One branch per issue, always started from an up-to-date `main`.
- Name: `#<n>-` followed by the issue title in lowercase, with every run of characters other than `a-z` and `0-9` replaced by a single `-`, and no leading or trailing `-`:
  - #1 "Hard over-temperature cutoff" → `#1-hard-over-temperature-cutoff`
  - #10 "Selectable temperature step (1/5/10)" → `#10-selectable-temperature-step-1-5-10`
- `#` starts a comment in the shell, so always quote the name: `git switch '#1-hard-over-temperature-cutoff'`.
- Before starting a new issue, the previous issue's PR should be merged. If it isn't, the choice is: wait, merge it first, or (only if the new issue depends on it) branch off the unmerged branch.

### Commits

- Format: `#<n> <issue title>: <topic>`, a blank line, then a `* ` bullet list. Claude writes the messages in `/commit-issue`; the details of the format are in `docs/commit-messages.md`.
- Every commit is self-contained: CI must be able to build and test it successfully. So the uncommitted work normally becomes one commit, whatever subjects it touches; it's only split when you ask (because it's too big), and then only into commits that each build and pass the tests.
- Nothing is committed without your review: Claude stages each commit, shows its file list and message, and waits for your OK before committing. The staged changes are exactly that commit, so you can view its diff in your own diff viewer first.
- Every commit is pushed right after it's made, so it's never amended or force-pushed.

### Pull requests

- Merged with **Squash and merge**: `main` gets one commit per issue, and its message is the PR's title and description. The topic commits stay visible in the closed PR.
- So the PR title and description *are* the final commit message, with the same structure as the topic commits. The `pr-description` agent writes the PR description (format in `docs/commit-messages.md`), Claude checks it and creates the PR as a GitHub **draft PR**. You review it, mark it *Ready for review* and merge it with *Squash and merge*:
  - Title: `#<n> <issue title>` (no topic: it covers the whole issue).
  - Description: the bullets of the issue's topic commits with the irrelevant ones removed, then `Closes #<n>`:

    ```
    * <change>
    * <change>

    Closes #<n>
    ```

    Which commit bullets are irrelevant is listed in `docs/commit-messages.md` (in short: fixes, tests, anything superseded, stale notes, housekeeping; "Also:" items and still-true "NOTE:" items stay).
  - The repository must use the PR title and description as the squash commit message: Settings → General → Pull Requests → squash merging → "Default to pull request title and description".
- The draft PR is opened right after the issue's first commit, without reviewers and with a placeholder description (plus `Closes #<n>`), so CI runs on every push while work continues. The real title and description are only written after the hardware tests are confirmed. The test record isn't part of the description, since that ends up on `main`.
- After the merge: switch to the updated `main`, delete the local issue branch, and check that the issue is closed and *Done* on the board.

## Tool notes

- In Claude Code's Git Bash, `gh` (GitHub CLI) is not on the PATH: prefix commands with `export PATH="$PATH:/c/Program Files/GitHub CLI";`.
- Building and uploading is done by you, in VS Code's PlatformIO sidebar. Claude doesn't run `pio`.

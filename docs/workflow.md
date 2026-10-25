# Development workflow

How work on this project goes from a GitHub issue to `main`. Each phase has a Claude Code skill (in `.claude/skills/`) that follows these rules. This document is the single source for the conventions: the skills refer to it instead of repeating them.

| Phase | Skill | What happens |
|---|---|---|
| Start | `/start-issue <n>` | Branch from an up-to-date `main`, issue to *In Progress* on the board, short plan for approval |
| Work | (normal conversation) | Implement, you build and test on the device |
| Commit | `/commit-issue <n>` | Checks, topic commits with messages from the `commit-message` agent |
| Finish | `/finish-issue <n>` | Confirm hardware tests, push, create a draft PR; after you've merged it: clean up |

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

- Format: `#<n> <issue title>: <topic>`, a blank line, then a `* ` bullet list. Messages are written by the `commit-message` agent (`.claude/agents/commit-message.md`), which owns the details of the format.
- Each commit covers one topic; work that spans topics becomes several commits.
- Nothing is committed without your review: Claude stages each commit, shows its changes and message, and waits for your OK before committing.
- Nothing is pushed without being asked.

### Pull requests

- Merged with **Squash and merge**: `main` gets one commit per issue, and its message is the PR's title and description. The topic commits stay visible in the closed PR.
- So the PR title and description *are* the final commit message, with the same structure as the topic commits. The `commit-message` agent writes it as a "squash message", Claude checks it and creates the PR as a GitHub **draft PR**. You review it, mark it *Ready for review* and merge it with *Squash and merge*:
  - Title: `#<n> <issue title>` (no topic: it covers the whole issue).
  - Description: the bullets of the issue's topic commits with the irrelevant ones removed, then `Closes #<n>`:

    ```
    * <change>
    * <change>

    Closes #<n>
    ```

    Irrelevant are: fixes for QA or review comments, build fixes and typo fixes; added tests; anything reverted, superseded or made redundant by a later commit on the branch (only the end result counts); notes that no longer hold at the end of the branch (e.g. "not yet tested" when it was tested later); pure housekeeping (formatting, line endings, comment rewording). Duplicates are merged into one bullet. "Also:" items and still-true "NOTE:" items stay.
  - The repository must use the PR title and description as the squash commit message: Settings → General → Pull Requests → squash merging → "Default to pull request title and description".
- A PR is only drafted after the hardware tests are confirmed. The test record isn't part of the description, since that ends up on `main`.
- After the merge: switch to the updated `main`, delete the local issue branch, and check that the issue is closed and *Done* on the board.

## Tool notes

- In Claude Code's Git Bash, `gh` (GitHub CLI) is not on the PATH: prefix commands with `export PATH="$PATH:/c/Program Files/GitHub CLI";`.
- Building and uploading is done by you, in VS Code's PlatformIO sidebar. Claude doesn't run `pio`.

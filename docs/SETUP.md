# New repository checklist

Template generation copies files; it is not a repository settings policy. Review
these items in every generated repository, including organization defaults. This
starter does not create rulesets, change access permissions, provision credentials,
or enable paid services.

## General settings

- Confirm the owner, repository name, visibility, description, and default branch.
  Use `main` unless the project needs another name.
- Under **Settings → General → Pull Requests**, enable squash merging and disable
  merge commits and rebase merging. Select **Pull request title and description**
  for the default squash message. API equivalents: `allow_squash_merge=true`,
  `allow_merge_commit=false`, `allow_rebase_merge=false`,
  `squash_merge_commit_title=PR_TITLE`, `squash_merge_commit_message=PR_BODY`.
- Enable **Automatically delete head branches** (`delete_branch_on_merge=true`).
- Keep **Template repository** enabled on the reusable template. A normal project
  generated from it usually should not itself be marked as a template.

Squashing makes a PR one commit on the target branch, even if it contains hundreds
of working commits. This does not clean up history already merged or prevent
direct pushes. See [GitHub's squash settings](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/configuring-commit-squashing-for-pull-requests).

## Checks and access

- Confirm Actions is available under the account's existing policy and that the
  first **CI** run succeeds. Review usage limits without enabling paid capacity.
- With the owner's explicit approval, configure a `main` ruleset or branch
  protection to require PRs and the exact check **Repository checks**. Select the
  GitHub Actions check after it has run. Add review requirements appropriate to
  the team; requiring another reviewer may block a solo project. Availability for
  private repositories depends on the GitHub plan. No such rule is installed by
  this template.
- Review collaborators, workflow permissions, allowed actions, environments,
  secrets, and security features separately. Do not assume these settings or
  protections are copied. Any changes need the owner's explicit authorization.
- No credentials are needed for the supplied check. Checkout uses a read-only
  job token and `persist-credentials: false`; do not add a personal token.
- Choose a license and add a private security-reporting contact before public
  release. Review dependencies' licenses when choosing the application stack.

## Extend CI with your stack

Keep `.github/workflows/ci.yml` and its **Repository checks** job name stable.
Add actual stack validation steps after the repository check for the simplest
setup, or add uniquely named jobs for separate feedback. When splitting jobs,
require each relevant job in branch protection; a green repository check alone
does not cover those jobs.

Keep required workflows triggered for every PR. Do not add `paths`, `paths-ignore`,
or branch filters that exclude supported PRs: skipped workflows can leave required
checks pending. Do not use `[skip ci]` in commits requiring checks. Keep
`merge_group` if a merge queue is used. The starter runs the PR merge result on
`pull_request` and the pushed commit on `push`.

Prefer always-running checks initially. If conditional jobs become necessary, add
an always-running final gate that explicitly fails when required dependencies
fail, are cancelled, or are unexpectedly skipped, and require that gate. Do not
silently treat missing validation as success.

Pin external actions to full commit SHAs with release comments, review Dependabot
updates, and use minimum workflow permissions. See
[GitHub workflow syntax](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax)
and [checkout configuration](https://github.com/actions/checkout).

# Project template

A small, general-purpose starting point for Michael-Wester's projects. Bring your
own language, framework, and deployment target.

Includes contributor and agent guidance, bug and feature forms, a PR template,
editor/Git conventions, and CI that validates the starter's real documentation and
configuration. It has no application code, product tests, secrets, or deployment.

## Start a project

1. Click **Use this template → Create a new repository** on GitHub. Select the
   owner and name, keep it private initially, and copy only the default branch.
2. Clone the new repository and replace this README with the project's purpose,
   prerequisites, setup, usage, and validation commands.
3. Choose your stack. Add source, dependency manifests and lockfiles, useful tests,
   and stack-specific ignores. Update [AGENTS.md](AGENTS.md) with actual commands.
4. Complete the [repository settings checklist](docs/SETUP.md). These settings
   must be checked for each new repository; the template files do not enforce them.
5. Choose an appropriate license before public release. No open-source license is
   granted by this starter, and no LICENSE file is included automatically.

## Check the starter

Requires Git and Python 3.10+; Python is only a repository tooling choice. CI uses
the Python supplied by GitHub's Ubuntu 24.04 runner and one pinned dependency,
PyYAML, for parsing configuration.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-checks.txt
.venv/bin/python scripts/check_repository.py
```

On Windows, use `py -3 -m venv .venv` and `.venv\Scripts\python.exe` in place of
`.venv/bin/python`. The check reads tracked files; stage new files with `git add`
before running it. It validates UTF-8 text conventions, JSON/YAML syntax (including
duplicate keys), and local inline Markdown file links. It does not check remote
URLs, Markdown anchors/reference links, or full GitHub configuration schemas.

CI runs on pushes, pull requests, merge groups, and manual dispatches. Its stable
check name is **Repository checks**. It proves repository hygiene only. Add real
lint, type, build, and test commands when there is application code; see
[extending CI](docs/SETUP.md#extend-ci-with-your-stack).

## Keep it useful

- See [CONTRIBUTING.md](CONTRIBUTING.md) for the short contribution workflow.
- Dependabot checks GitHub Actions and the check tooling monthly. Add ecosystems
  only when their actual dependency manifests exist.
- Remove starter guidance that no longer applies. Add documents and directories
  when the project needs them, rather than maintaining empty scaffolding.

GitHub templates produce a new repository with its own history. Later edits to
this template do not automatically update projects created from it. See
[GitHub's template guide](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-repository-from-a-template).

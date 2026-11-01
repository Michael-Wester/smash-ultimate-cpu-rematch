# Working in this repository

This starter has no application stack yet. Read README.md, CONTRIBUTING.md, and
the files related to the requested change before editing. Follow more specific
AGENTS.md files in subdirectories when present.

- Keep changes focused on the request; preserve unrelated work.
- Follow existing conventions. Add dependencies or abstractions only when they
  solve a concrete need. Never invent product behavior or placeholder tests.
- Keep secrets, credentials, private data, and generated build output out of Git.
  Use sanitized environment examples and document new configuration.
- Run `python scripts/check_repository.py` with the check dependencies installed.
  Once a stack is chosen, run its relevant checks too. Report commands and results;
  distinguish checks that passed from checks that could not run.
- Add regression coverage for behavior changes where useful. Update documentation
  when commands, configuration, or user-facing behavior change.
- Preserve the `Repository checks` CI job name and unfiltered workflow triggers.
  Add real stack checks before treating CI as proof of application correctness.
- Do not force-push, rewrite shared history, change repository permissions or
  rules, publish, deploy, or merge without explicit authorization for that action.
- Before finishing, inspect the diff for unintended files and summarize the
  change, validation, and any remaining limitations.

When adapting this template, replace this introduction with the project's purpose
and add its exact setup, lint, build, and test commands here.

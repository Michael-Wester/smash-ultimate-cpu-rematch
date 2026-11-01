# CPU Rematch

Skyline C++ plugin for Smash Ultimate 13.0.3 on Eden. Read README.md for supported
behavior and docs/DEVELOPMENT.md for setup and validation commands.

- Preserve exact-version checks, memory validation and local 1v1 CPU restrictions.
- The released NRO and install ZIP are tracked intentionally; keep intermediate
  output in ignored `build/`. Update release artifacts only after validation.
- Stage new files, then run `python scripts/check_repository.py` and
  `python source/run_checks.py`. Report simulated tests separately from live-game tests.
- Keep the `Repository checks` CI job name and unfiltered workflow triggers.
- Preserve license and upstream attribution. Exclude logs, keys and game assets.

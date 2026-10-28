# Contributing

This project uses **remote-only development builds and tests**. Do not add pre-commit hooks, editor tasks, or installation scripts that automatically compile on a contributor's machine.

1. Make focused source changes in a branch and open a pull request.
2. GitHub Actions builds the desktop game on Linux, Windows, and macOS and runs headless tests, sanitizers, renderer screenshots, and captured-audio checks.
3. Download renderer evidence and inspect the screenshots. For interactive playtesting, use the CI-built binaries; automated smoke checks do not replace human judgment.
4. Update tests when changing mechanics and document honest scope/limitations. Keep proprietary game assets, downloaded copyrighted music, credentials, and personal saves out of the repository.
5. Keep the core simulation free of raylib and keep visual/audio randomness separate from simulation RNG.

The **Remote source formatting** workflow runs clang-format 18 on a GitHub-hosted runner and commits style changes to the selected branch. It is manual because it writes source. A formatting commit made using `GITHUB_TOKEN` does not trigger CI automatically; dispatch **CI** afterward, or push a release tag to verify that exact revision.

A release tag is publishable only when the full desktop matrix and sanitizer job succeed. Do not weaken tests merely to get a green badge. Bug reports should include platform, package/revision, seed, reproduction steps, and relevant screenshots/logs—never access tokens or private information.

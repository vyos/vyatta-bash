# AGENTS.md

## Project purpose

GNU Bash 4.1 fork patched with Vyatta CLI semantics. Provides `vbash` (the Vyatta/VyOS configuration shell) — the user-facing shell that drives `set`/`commit`/`show` flow on VyOS. Massive C codebase inherited from GNU Bash; VyOS changes are layered on top. Ships as the `vyatta-bash` Debian package.

## Tech stack

- C (GNU Bash 4.1 baseline).
- GNU autotools (`Makefile.in`, `aclocal.m4`, `configure`).
- Debian packaging in `debian/`.
- License: GPL-3.0 (per `COPYING`/`LICENSE`).

## Build / test / run

```sh
./configure
make
dpkg-buildpackage -us -uc
```

There is no in-tree test runner specific to VyOS; the upstream Bash test suite under `tests/` exists but is not routinely run by CI.

## Repository layout

- C source files at root (`alias.c`, `array.c`, `arrayfunc.c`,...) — the upstream Bash 4.1 source tree.
- `debian/` — VyOS-specific Debian packaging (`vyatta-bash` source + binary package name).
- `CODEOWNERS`, `README` (upstream Bash README), `MANIFEST`, etc.

## Cross-repo context

Pre-dep of the legacy Vyatta config layer (`vyatta-cfg`, `vyatta-cfg-system` and the various `vyatta-op*`, `vyatta-config-*` Perl glue repos in `the private side`). Listed in an internal repository. Pulled into the ISO via `vyos/vyos-build`. New CLI work generally lives in `vyos-1x` Python rather than this shell.

## Conventions

- Default branch `rolling`. LTS branches when needed.
- Commit / PR title format: `component: T12345: description` (Phorge task ID at https://vyos.dev).
- Active workflows: `add-pr-labels.yml`, `auto-author-assign.yml`, `chceck-pr-message.yml` *(typo preserved)*, `check-pr-conflicts.yml`, `check-stale.yml`, `cla-check.yml`, `codeql.yml`, `pr-mirror-repo-sync.yml`, `trigger-rebuild-repo-package.yml`. Mirror pipeline IS wired up.
- Treat as upstream-vendored Bash; minimise diffs against the GNU Bash 4.1 baseline.

## Notes for future contributors

- Filename `chceck-pr-message.yml` is a known typo; left in place to avoid breaking the workflow ref. Leave as-is unless coordinated.
- Vyatta-specific behaviour patches are scattered across multiple `.c` files (no single "vyatta" subdir). Use `git log -- <file>` to see VyOS-era changes vs upstream.
- This is GPL-3.0 (Bash); the surrounding VyOS code mostly is GPL-2.0 — ABI/binary linkage is fine, source mixing is not.

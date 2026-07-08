# Unify Workflow Checklist (Generals/GeneralsMD → Core)

Condensed steps for porting duplicated engine code from `Generals/`/`GeneralsMD/`
into the shared `Core/` tree using `unify_move_files.py`.

1. **Diff** the file(s) between `Generals/` and `GeneralsMD/` (ignore the
   copyright banner line — `Generals(tm)` vs `Generals Zero Hour(tm)` is
   expected) to find what's actually diverged.

2. **Port** Zero Hour's version into the Generals copy until the diff is
   empty (banner aside). Zero Hour takes precedence per `CONTRIBUTING.md`.

3. **Document the port**: in the merge commit body (or a code comment if
   non-obvious), note exactly which function(s)/blocks were copied from
   GeneralsMD, so a reviewer doesn't have to re-diff to see the scope.

4. **Compile both `generalsv.exe` and `generalszh.exe`.** A clean diff only
   proves the two files match — it doesn't prove every symbol the ported
   code calls actually exists on both sides (e.g. a function declared in a
   shared header but only ever implemented in GeneralsMD's `.cpp`).

5. Commit: `unify(x): Merge <thing> from Zero Hour`.

6. **Add** `unify_file(Game.ZEROHOUR, "<path>", Game.CORE, "<path>")` lines to
   `unify_move_files.py`'s `main()` for every file now identical, **run** the
   script, then **re-comment** those lines afterward (the file is an
   append-only log of every unify ever performed — don't delete old entries,
   don't leave new ones active).

7. **Compile both games again** — confirms the move didn't break a
   `CMakeLists.txt` entry or leave a dangling reference.

8. Commit: `unify(x): Move <thing> files to Core`.

Two commits, two compiles, one documented diff.

# miniFlashDataBase v1.91

Adds interactive shell with day/sector navigation and summaries.

## Use

1. Upload `tests/shell_demo_v1_91.ino`.
2. In Serial monitor (@115200) try commands:
   - `ls` / `ls day <YYYY-MM-DD>` / `ls sectors`
   - `cd day <...>` / `cd sector <...>`
   - `print`, `info`
   - `pf`, `stats`, `factory`, `gc`, `reset`

## Notes
- Shell uses `handleCommand()`; fallback commands show how to plug in custom logic.
- Day/sector summaries cached for quick navigation.

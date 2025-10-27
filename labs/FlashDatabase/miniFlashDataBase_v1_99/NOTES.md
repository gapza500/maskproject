# Regression Checklist

Before tagging v1.99, run the following manual checks:

1. Flash `miniFlashDataBase_v1_96_tests.ino` and capture Serial output showing
   `q latest`, `q day`, `export` with pagination tokens.
2. Flash `miniFlashDataBase_v1_99.ino` and verify:
   - `help` lists all commands.
   - `gc` handles erase intent without corruption.
   - RTC fallback still blocks appends when time invalid.
3. Exercise new predicate features (once implemented) and document results.

# miniFlashDataBase v1.92

Query engine release (JSONL/CSV output, includeKeys, sampling, shell `q`).

## Use

1. Upload `tests/query_demo_v1_92.ino`.
2. Serial @115200 should show:
   - `[test] JSONL latest` block with trimmed fields (`ts`, `temp`).
   - `[test] CSV range` block sampling every second record.
   - Shell demo (`fmt jsonl` + `q latest 5`).

## Notes
- `QuerySpec` supports `ts_from/to`, `day_from/to`, `includeKeys`, `sample_every`, `max_records`, `out`.
- Shell commands: `q latest N`, `q day YYYY-MM-DD`, `q range A..B`, `fmt`, `set csv`.

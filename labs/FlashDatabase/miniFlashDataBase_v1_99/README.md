# miniFlashDataBase v1.99

Goals:

- **Query predicates** – allow basic comparisons (`field<=value`, `>`, `!=`) in
  `queryLogs`/`export` so servers can prefilter before download.
- **Persisted anchors** – store day/sector anchors across boots and rebuild
  lazily, reducing startup scans.
- **Docs/tests** – extend examples to cover predicate usage and ensure anchors
  survive power cycles.

Before finalizing, rerun the v1.96 test harness and capture shell output (`help`,
`gc`, `export 2 token=...`) to confirm no regressions.

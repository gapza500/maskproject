# Shell Tour & GC Walkthrough

This transcript captures a typical session using the built-in FlashLogger shell.

```
> help
Commands:
  help                                  Show this summary
  ls                                    List known days
  ls sectors [day]                      List sectors for a day
  cd day <date|id|#n>                   Select day
  cd sector <id|#n>                     Select sector
  print / info                          Dump selected data/info
  q latest <N> [token=...]              Query latest records
  q day <YYYY-MM-DD> [token=...]
  q range <YYYY-MM-DD..YYYY-MM-DD> [token=...]
  export <N> [token=...]                Stream from cursor (auto-saves)
  cursor show|clear|set|save|load       Manage cursor state
  fmt csv|jsonl                         Set output format
  set csv <cols>                        Configure CSV columns
  pf | stats | factory | gc             Format, stats, maintenance
  reset <code>                          Factory reset logs
  scanbad                               Scan/quarantine bad sectors

> ls
#  DATE        DAYID  SECT  BYTES     STATUS  RANGE
0  01/01/25    9132   1     315       OPEN    789048683..789048923

> cd day 9132
[DAY] 01/01/25  dayID=9132  sectors=1  bytes=315  status=OPEN

> info
[DAY] 01/01/25  dayID=9132  sectors=1  bytes=315  status=OPEN

> stats
Total: 16.00 MB  Used: 0.31 MB  Free: 15.69 MB  Used: 1.9%  Health: 99.9%  EstDays: 1825

> cursor show
cursor: day=9132 sector=55 addr=0x037287 seq=6

> export 3
{"ts":1735733543,"temp":26.0,"bat":18.0,"hb":1}
{"ts":1735733603,"temp":27.5,"bat":15.0,"hb":1}
{"ts":1735733663,"temp":29.0,"bat":22.0,"hb":1}
(3 rows)

> gc
🧹 GC: checking sectors...
  erased sector 10 (day=9130, gen=81)
```

Use this script as a reference when documenting or demoing the CLI.

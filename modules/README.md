# Modules Directory

This folder groups reusable hardware modules and board profiles. Each board
profile now keeps its pin assignment as both:

- a table in `README.md` for quick reference, and
- a C++ header that exposes the pins via a dedicated namespace.

Include the header that matches your target board, for example:

```cpp
#include "../../../modules/cytron_maker_feather_aiot_s3/pins_cytron_maker_feather_aiot_s3.h"
#include "../../../modules/max17048/Max17048.h"

using namespace board_pins::cytron_maker_feather_aiot_s3;

Max17048 fuel;
if (fuel.begin()) {
  float soc = fuel.readPercent();
}
```

When adding a new hardware revision or board, follow the same structure:
create a subfolder, document the pins in a README, and provide a header with
strongly named constants inside the `board_pins` namespace.

> Heads up: keep directory names filename-safe (lowercase + underscores) so the
> Arduino build system can resolve the `#include` paths without issue. Legacy
> folders with spaces are retained for reference, but new code should include
> the sanitized paths. Adjust the relative prefix (`../../../`) to match the
> structure of your sketch folder.

# Modules Directory

This folder groups reusable hardware modules and board profiles. Each board
profile now keeps its pin assignment as both:

- a table in `README.md` for quick reference, and
- a C++ header that exposes the pins via a dedicated namespace.

Include the header that matches your target board, for example:

```cpp
#include "modules/Cytron Maker Feather AIoT S3/pins_cytron_maker_feather_aiot_s3.h"

using namespace board_pins::cytron_maker_feather_aiot_s3;

// use FLASH_SCK, FLASH_MISO, etc.
```

When adding a new hardware revision or board, follow the same structure:
create a subfolder, document the pins in a README, and provide a header with
strongly named constants inside the `board_pins` namespace.


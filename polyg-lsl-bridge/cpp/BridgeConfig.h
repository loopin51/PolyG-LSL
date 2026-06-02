#pragma once
// Mirror these values with config.toml. v1 does not parse TOML in C++.
// IMPORTANT: BRIDGE_PGA_GAIN_IDX must equal [device].gain_idx in config.toml,
// or the device gain and the Python uV scaling will disagree (wrong uV output).
#define BRIDGE_HOST         "127.0.0.1"
#define BRIDGE_PORT         51234
#define BRIDGE_PGA_GAIN_IDX 9

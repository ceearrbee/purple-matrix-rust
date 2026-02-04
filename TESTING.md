# Testing the Purple Matrix Rust Plugin

## Overview
This project contains two types of tests:
1.  **Rust Unit Tests:** verify the logic within the `matrix-sdk` backend wrapper (`src/lib.rs`).
2.  **C Logic Tests:** verify the Libpurple plugin logic (`plugin.c`), specifically the buddy list grouping and threading logic, using a mock Libpurple implementation.

## Running Rust Tests
Standard cargo testing applies:
```bash
cargo test
```
*Note: This currently runs the unit tests defined in the `tests` module in `src/lib.rs`.*

## Running C Logic Tests
We use a mock header set to verify `plugin.c` logic without needing a full Libpurple installation or GUI.

Compile and run the tests:
```bash
gcc -I. -Itests -Itests/libpurple tests/test_logic.c $(pkg-config --cflags --libs glib-2.0) -o run_tests
./run_tests
```

### Coverage
The C test (`test_logic.c`) specifically covers:
*   **Thread Grouping:** verifying that threads are correctly grouped under their parent Room Name (if known) or fallback ID.
*   **Buddy List Management:** verifying that groups and chats are created/found as expected.

## Future Work
*   Expand Rust tests to cover E2EE store initialization (requires mocking the store).
*   Expand C tests to cover Slash Commands parsing (`cmd_thread`).

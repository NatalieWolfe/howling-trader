# Style Guide

Follow the style applied by Clang Format according to the rules described in the
.clang-format configuration file. Below are examples for specific cases to help
guide style.

## Snake Case
All C++ identifiers (functions, variables, members) and Bazel targets must use
`snake_case`. C++ classes also use `snake_case`.

**Good:**
```cpp
class postgres_database;
void save_candle();
int schema_version;
```
**Bad:**
```cpp
void SaveCandle();
int SchemaVersion;
```

## Variable Names
Use full words for variable names. Avoid abbreviations and single-letter
variable names except for well-established patterns (e.g. `itr`, `i`, `j`, `k`
for loop iterators). Use a leading underscore `_` for all private members,
including private member variables, functions, structs, and classes.

**Good:**
```cpp
int retry_count;
for (int i = 0; i < 10; ++i);
auto itr = buffer.begin();
class my_class {
 private:
  struct _entry {};
  void _helper();
  int _member_variable;
};
```
**Bad:**
```cpp
int ret_cnt;
int r;
auto it = buffer.begin();
class my_class {
 private:
  struct entry {};
  void helper();
  int member_variable;
};
```

## Capital Snake Case
Global constants and enum values must use `CAPITAL_SNAKE_CASE`.

**Good:**
```cpp
constexpr int DEFAULT_SCHEMA_VERSION = 1;
enum class action { BUY, SELL };
```
**Bad:**
```cpp
constexpr int default_schema_version = 1;
```

## Pascal Case
Test fixtures, test case names, and Protobuf message names must use `PascalCase`.

**Good:**
```cpp
class PostgresDatabaseTest : public DatabaseTest {};
TEST_F(PostgresDatabaseTest, InitializesEmptyDatabase) {};
// In .proto
message TradeRecord {};
```
**Bad:**
```cpp
class postgres_database_test;
TEST_F(postgres_database_test, initializes_empty_database);
```

## Protobuf
Messages use `PascalCase`, while fields use `snake_case`.

**Good:**
```proto
message MarketUpdate {
  double bid_price = 1;
}
```

## Pragma Once
Use `#pragma once` for all header guards.

## Header Cleanliness
Do not use `using` declarations or `using namespace` in header files. Always
fully qualify types or matchers.

## Absolute Includes
Include all project files using their full path from the workspace root.

**Good:**
```cpp
#include "services/db/schema/schema.h"
```

## Raw SQL
Use raw string literals with the `sql` delimiter for SQL queries.

**Good:**
```cpp
R"sql(
  SELECT refresh_token FROM auth_tokens
)sql"
```

## Modern C++
Prefer `std::generator`, `std::future`, and `std::string_view` where
appropriate. Prefer `std::unique_ptr` and `std::shared_ptr` over raw `new`/
`delete`. Use `[[nodiscard]]` for functions where ignoring the return value is
likely a bug (e.g., `empty()`, `size()`).

## Thread Safety
Classes designed for multi-threaded use should explicitly state their
thread-safety guarantees (e.g., "thread-safe," "thread-compatible," or
"internally synchronized").

Use `std::atomic` only for simple, independent counters. For complex state
transitions (e.g., coordinating head and tail in a circular buffer), prefer
`std::mutex` to ensure consistency across multiple variables.

## Bazel Naming
All Bazel targets must be `snake_case`.

## Line Length
Limit all lines to 80 columns.

## Indentation
Use 2 spaces for indentation. Never use tabs. For wrapped lines (continuation
indentation), use 4 spaces.

**Good:**
```cpp
void function() {
  int result = very_long_function_name_causing_parameters_to_wrap(
      parameter_one, parameter_two);
}
```

## Curly Braces
Always use curly braces for blocks (if, for, while, functions) unless the entire
statement fits on a single line. Single-line bodies that fit on the same line as
their condition do not need curly braces around them. When braces are used, they
should open on the same line.

**Good:**
```cpp
if (condition) {
  do_something();
}

void function() {
  ...
}
```

## Comments
All comments should use full sentences with correct grammar, spelling, and
punctuation.

**Good:**
```cpp
// The circular buffer is full. Wrap around to the beginning.
if (full()) {
  _head = 0;
}
```
**Bad:**
```cpp
// wrap around
if (full()) {
  _head = 0;
}
```

## Documentation
Use Doxygen-style comments with `/** */` for documenting functions.

**Good:**
```cpp
/**
 * @brief Saves a candle to the database.
 */
void save_candle(const Candle& candle);
```
**Bad:**
```cpp
// Saves a candle to the database.
void save_candle(const Candle& candle);
```

When a `@param` description wraps lines, all `@param` descriptions in that
block should be formatted with their description on the next line indented by 2
spaces:

```cpp
/**
 * @brief Some description.
 *
 * @param foo
 *  Some extra long comment that would not have fit within the 80 character
 *  limit on the same line as the param tag.
 * @param bar
 *  Short description that would have fit.
 */
```

## Single-line Blocks
Simple if/for/while statements may be written on a single line without braces if
they are short and clear. Single-line bodies that fit on the same line as their
condition do not need braces around them.

**Good:**
```cpp
if (!exists) return;
for (const auto& item : items) process(item);

// Wrapped even though they'd fit on one line.
if (condition) {
  do_something();
} else {
  do_something_else();
}
```
**Bad:**
```cpp
if (!exists) { return; } // Single-line body on same line does not need braces.

if (condition)
  do_something(); // Body on new line requires braces.

// Compound conditions should be considered on a whole, this should be wrapped
// in braces.
if (first_condition) do_something();
else if (second_condition) do_other_thing();
else do_something_else();
```

## Auto
Avoid using `auto` when the type is not difficult to determine or specify.
`auto` is acceptable when the type is explicit (or nearly explicit) on the
right-hand side (such as `std::make_shared<x>()` or `get_service<x>()`), when
the exact type cannot be named (such as lambdas), or when it is overly verbose
and redundant (such as complex iterator types).

**Good:**
```cpp
auto entry = std::make_shared<callback_entry>();
auto& db = registry.get_service<database>();
auto itr = buffer.begin();
const std::chrono::steady_clock::time_point now =
    std::chrono::steady_clock::now();
```
**Bad:**
```cpp
const auto now = std::chrono::steady_clock::now();
auto count = buffer.size();
```

## Parameter Validation
Validate function parameters and throw exceptions rather than silently accepting
or ignoring invalid inputs.

**Good:**
```cpp
if (!callback) {
  throw std::invalid_argument("Callback must not be empty.");
}
```
**Bad:**
```cpp
if (!callback) return {};
```

## Parameter Ordering
When a function or constructor accepts an `options` parameter, place it as the
last parameter. Only lambdas (callbacks) may be specified after `options`.

**Good:**
```cpp
void start_stream(
    const std::string& symbol, const stream_options& options = {});
void fetch_candles(
    const std::string& symbol,
    const fetch_options& options,
    std::function<void(const candle&)> callback);
```
**Bad:**
```cpp
void start_stream(
    const stream_options& options, const std::string& symbol);
void fetch_candles(
    const fetch_options& options,
    const std::string& symbol,
    std::function<void(const candle&)> callback);
```

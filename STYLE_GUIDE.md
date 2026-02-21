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
for loop iterators). Use a leading underscore `_` for private member variables.

**Good:**
```cpp
int retry_count;
for (int i = 0; i < 10; ++i);
auto itr = buffer.begin();
class my_class {
 private:
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
statement fits on a single line. Braces should open on the same line.

**Good:**
```cpp
if (condition) {
  do_something();
}

void function() {
  ...
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

## Single-line Blocks
Simple if/for/while statements may be written on a single line without braces if
they are short and clear.

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
if (condition)
  do_something(); // Body on new line requires braces.

// Compound conditions should be considered on a whole, this should be wrapped
// in braces.
if (first_condition) do_something();
else if (second_condition) do_other_thing();
else do_something_else();
```

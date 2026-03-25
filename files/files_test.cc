#include "files/files.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "gtest/gtest.h"

namespace howling::files {

namespace fs = ::std::filesystem;

TEST(FilesTest, ReadFileSucceeds) {
  const fs::path path = "test_file.txt";
  const std::string content = "Hello, world!";

  {
    std::ofstream file(path);
    file << content;
  }

  EXPECT_EQ(read_file(path), content);

  fs::remove(path);
}

TEST(FilesTest, ReadFileThrowsOnMissingFile) {
  EXPECT_THROW(read_file("/non/existent/file"), std::runtime_error);
}

TEST(FilesTest, WriteFileSuccess) {
  const fs::path path = "test_file.txt";
  const std::string content = "Test content";

  ASSERT_FALSE(fs::exists(path));
  write_file(path, content);
  ASSERT_TRUE(fs::exists(path));
  EXPECT_EQ(read_file(path), content);

  fs::remove(path);
}

} // namespace howling::files

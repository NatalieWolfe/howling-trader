#include "files/files.h"

#include <fstream>
#include <stdexcept>

#include "gtest/gtest.h"

namespace howling::files {

TEST(FilesTest, ReadFileSucceeds) {
  const std::filesystem::path path = "test_file.txt";
  const std::string content = "Hello, world!";

  {
    std::ofstream file(path);
    file << content;
  }

  EXPECT_EQ(read_file(path), content);

  std::filesystem::remove(path);
}

TEST(FilesTest, ReadFileThrowsOnMissingFile) {
  EXPECT_THROW(read_file("/non/existent/file"), std::runtime_error);
}

} // namespace howling::files

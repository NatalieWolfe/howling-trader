#include "logs/json_sink.h"

#include <iostream>

#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "json/json.h"
#include "strings/format.h"
#include "strings/json.h"

namespace howling {

void JsonLogSink::Send(const absl::LogEntry& entry) {
  Json::Value root;
  root["level"] = absl::LogSeverityName(entry.log_severity());
  root["ts"] = to_string(absl::ToChronoTime(entry.timestamp()));
  root["message"] = std::string(entry.text_message());
  root["file"] = std::string(entry.source_filename());
  root["line"] = entry.source_line();

  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  std::cout << Json::writeString(builder, root) << std::endl;
}

void JsonLogSink::Flush() { std::cout << std::flush; }

} // namespace howling

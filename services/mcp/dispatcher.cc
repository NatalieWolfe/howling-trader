#include "services/mcp/dispatcher.h"

#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "strings/json.h"
#include "json/value.h"

namespace howling::mcp {
namespace {

Json::Value make_result_response(const Json::Value& id, Json::Value result) {
  Json::Value response(Json::objectValue);
  response["jsonrpc"] = "2.0";
  response["id"] = id;
  response["result"] = std::move(result);
  return response;
}

Json::Value make_error_response(
    const Json::Value& id,
    int code,
    std::string_view message,
    std::optional<Json::Value> data = std::nullopt) {
  Json::Value error(Json::objectValue);
  error["code"] = code;
  error["message"] = std::string(message);
  if (data.has_value()) error["data"] = std::move(*data);

  Json::Value response(Json::objectValue);
  response["jsonrpc"] = "2.0";
  response["id"] = id;
  response["error"] = std::move(error);
  return response;
}

bool is_valid_jsonrpc_id(const Json::Value& id) {
  return !id.isBool() && (id.isString() || id.isIntegral() || id.isNull());
}

Json::Value make_tool_call_result(std::string_view text, bool is_error) {
  Json::Value text_item(Json::objectValue);
  text_item["type"] = "text";
  text_item["text"] = std::string(text);

  Json::Value content(Json::arrayValue);
  content.append(std::move(text_item));

  Json::Value result(Json::objectValue);
  result["content"] = std::move(content);
  result["isError"] = is_error;
  return result;
}

} // namespace

mcp_dispatcher::mcp_dispatcher(server_info info)
    : _server_info(std::move(info)) {
  if (_server_info.name.empty()) {
    throw std::invalid_argument("Server name must not be empty.");
  }
  if (_server_info.version.empty()) {
    throw std::invalid_argument("Server version must not be empty.");
  }
}

void mcp_dispatcher::register_tool(std::unique_ptr<tool_handler> handler) {
  if (!handler) {
    throw std::invalid_argument("Tool handler must not be null.");
  }
  const std::string name(handler->name());
  if (name.empty()) {
    throw std::invalid_argument("Tool handler name must not be empty.");
  }
  if (_tool_index.contains(name)) {
    throw std::invalid_argument("Duplicate tool handler name: " + name);
  }
  tool_handler* raw_handler = handler.get();
  _tools.push_back(std::move(handler));
  _tool_index.emplace(name, raw_handler);
}

void mcp_dispatcher::register_resource(
    std::unique_ptr<resource_handler> handler) {
  if (!handler) {
    throw std::invalid_argument("Resource handler must not be null.");
  }
  const std::string uri(handler->uri());
  if (uri.empty()) {
    throw std::invalid_argument("Resource handler URI must not be empty.");
  }
  if (handler->name().empty()) {
    throw std::invalid_argument("Resource handler name must not be empty.");
  }
  if (_resource_index.contains(uri)) {
    throw std::invalid_argument("Duplicate resource handler URI: " + uri);
  }
  resource_handler* raw_handler = handler.get();
  _resources.push_back(std::move(handler));
  _resource_index.emplace(uri, raw_handler);
}

dispatch_result mcp_dispatcher::dispatch(std::string_view raw_json) const {
  Json::Value parsed;
  try {
    parsed = to_json(raw_json);
  } catch (const std::exception& error) {
    return dispatch_result{
        .response = make_error_response(
            Json::nullValue, JSONRPC_PARSE_ERROR, error.what())};
  }

  if (parsed.isArray()) {
    if (parsed.empty()) {
      return dispatch_result{
          .response = make_error_response(
              Json::nullValue,
              JSONRPC_INVALID_REQUEST,
              "Batch request must not be empty.")};
    }

    Json::Value responses(Json::arrayValue);
    for (const Json::Value& item : parsed) {
      dispatch_result item_result = dispatch_request(item);
      if (item_result.has_response()) {
        responses.append(std::move(*item_result.response));
      }
    }
    if (responses.empty()) return dispatch_result{};
    return dispatch_result{.response = std::move(responses)};
  }

  return dispatch_request(parsed);
}

dispatch_result
mcp_dispatcher::dispatch_request(const Json::Value& request) const {
  if (!request.isObject()) {
    return dispatch_result{
        .response = make_error_response(
            Json::nullValue,
            JSONRPC_INVALID_REQUEST,
            "Request must be a JSON object.")};
  }

  static const Json::Value NULL_VALUE(Json::nullValue);
  const bool has_id = request.isMember("id");
  const Json::Value& id = has_id ? request["id"] : NULL_VALUE;
  if (has_id && !is_valid_jsonrpc_id(id)) {
    return dispatch_result{
        .response = make_error_response(
            Json::nullValue,
            JSONRPC_INVALID_REQUEST,
            "Request id must be a string, integer, or null.")};
  }

  if (!request.isMember("jsonrpc") || !request["jsonrpc"].isString() ||
      request["jsonrpc"].asString() != "2.0") {
    return dispatch_result{
        .response = make_error_response(
            id, JSONRPC_INVALID_REQUEST, "Field 'jsonrpc' must be \"2.0\".")};
  }

  // Accept valid JSON-RPC 2.0 response objects sent by clients with no body.
  if (has_id && !request.isMember("method") &&
      (request.isMember("result") != request.isMember("error"))) {
    return dispatch_result{};
  }

  if (!request.isMember("method") || !request["method"].isString()) {
    return dispatch_result{
        .response = make_error_response(
            id, JSONRPC_INVALID_REQUEST, "Field 'method' must be a string.")};
  }

  // Any valid JSON-RPC 2.0 message without an "id" member is a notification.
  if (!has_id) return dispatch_result{};

  const std::string method = request["method"].asString();
  static const Json::Value EMPTY_OBJECT(Json::objectValue);
  const Json::Value& params =
      (request.isMember("params") && !request["params"].isNull())
      ? request["params"]
      : EMPTY_OBJECT;
  if (!params.isObject()) {
    return dispatch_result{
        .response = make_error_response(
            id, JSONRPC_INVALID_PARAMS, "Field 'params' must be an object.")};
  }

  try {
    if (method == "initialize") {
      return dispatch_result{
          .response = make_result_response(id, _handle_initialize())};
    }
    if (method == "ping") {
      return dispatch_result{
          .response = make_result_response(id, Json::Value(Json::objectValue))};
    }
    if (method == "tools/list") {
      return dispatch_result{
          .response = make_result_response(id, _handle_tools_list())};
    }
    if (method == "tools/call") {
      return dispatch_result{.response = _handle_tools_call(id, params)};
    }
    if (method == "resources/list") {
      return dispatch_result{
          .response = make_result_response(id, _handle_resources_list())};
    }
    if (method == "resources/read") {
      return dispatch_result{.response = _handle_resources_read(id, params)};
    }
  } catch (const std::exception& error) {
    return dispatch_result{
        .response =
            make_error_response(id, JSONRPC_INTERNAL_ERROR, error.what())};
  } catch (...) {
    return dispatch_result{
        .response = make_error_response(
            id, JSONRPC_INTERNAL_ERROR, "Internal server error.")};
  }

  return dispatch_result{
      .response = make_error_response(
          id, JSONRPC_METHOD_NOT_FOUND, "Method not found: " + method)};
}

Json::Value mcp_dispatcher::_handle_initialize() const {
  Json::Value capabilities(Json::objectValue);
  capabilities["tools"] = Json::Value(Json::objectValue);
  capabilities["resources"] = Json::Value(Json::objectValue);

  Json::Value info(Json::objectValue);
  info["name"] = _server_info.name;
  info["version"] = _server_info.version;

  Json::Value result(Json::objectValue);
  result["protocolVersion"] = std::string(MCP_PROTOCOL_VERSION);
  result["capabilities"] = std::move(capabilities);
  result["serverInfo"] = std::move(info);
  return result;
}

Json::Value mcp_dispatcher::_handle_tools_list() const {
  Json::Value tools(Json::arrayValue);
  for (const std::unique_ptr<tool_handler>& handler : _tools) {
    Json::Value entry(Json::objectValue);
    entry["name"] = std::string(handler->name());
    entry["description"] = std::string(handler->description());
    entry["inputSchema"] = handler->input_schema();
    Json::Value annotations = handler->annotations().to_json();
    if (!annotations.empty()) entry["annotations"] = std::move(annotations);
    tools.append(std::move(entry));
  }

  Json::Value result(Json::objectValue);
  result["tools"] = std::move(tools);
  return result;
}

Json::Value mcp_dispatcher::_handle_tools_call(
    const Json::Value& id, const Json::Value& params) const {
  if (!params.isMember("name") || !params["name"].isString()) {
    return make_error_response(
        id, JSONRPC_INVALID_PARAMS, "Missing or invalid tool 'name'.");
  }

  const std::string tool_name = params["name"].asString();
  auto itr = _tool_index.find(tool_name);
  if (itr == _tool_index.end()) {
    return make_error_response(
        id, JSONRPC_INVALID_PARAMS, "Unknown tool: " + tool_name);
  }

  static const Json::Value EMPTY_OBJECT(Json::objectValue);
  const Json::Value& arguments =
      (params.isMember("arguments") && !params["arguments"].isNull())
      ? params["arguments"]
      : EMPTY_OBJECT;
  if (!arguments.isObject()) {
    return make_error_response(
        id, JSONRPC_INVALID_PARAMS, "Tool 'arguments' must be an object.");
  }

  try {
    const tool_result result = itr->second->call(arguments);
    return make_result_response(
        id, make_tool_call_result(result.text, result.is_error));
  } catch (const std::exception& error) {
    return make_result_response(id, make_tool_call_result(error.what(), true));
  } catch (...) {
    return make_result_response(
        id, make_tool_call_result("Unknown tool error.", true));
  }
}

Json::Value mcp_dispatcher::_handle_resources_list() const {
  Json::Value resources(Json::arrayValue);
  for (const std::unique_ptr<resource_handler>& handler : _resources) {
    Json::Value entry(Json::objectValue);
    entry["uri"] = std::string(handler->uri());
    entry["name"] = std::string(handler->name());
    entry["description"] = std::string(handler->description());
    entry["mimeType"] = std::string(handler->mime_type());
    resources.append(std::move(entry));
  }

  Json::Value result(Json::objectValue);
  result["resources"] = std::move(resources);
  return result;
}

Json::Value mcp_dispatcher::_handle_resources_read(
    const Json::Value& id, const Json::Value& params) const {
  if (!params.isMember("uri") || !params["uri"].isString()) {
    return make_error_response(
        id, JSONRPC_INVALID_PARAMS, "Missing or invalid resource 'uri'.");
  }

  const std::string resource_uri = params["uri"].asString();
  auto itr = _resource_index.find(resource_uri);
  if (itr == _resource_index.end()) {
    Json::Value data(Json::objectValue);
    data["uri"] = resource_uri;
    return make_error_response(
        id,
        MCP_RESOURCE_NOT_FOUND,
        "Resource not found: " + resource_uri,
        std::move(data));
  }

  try {
    const std::string text = itr->second->read();
    Json::Value item(Json::objectValue);
    item["uri"] = std::string(itr->second->uri());
    item["mimeType"] = std::string(itr->second->mime_type());
    item["text"] = text;

    Json::Value contents(Json::arrayValue);
    contents.append(std::move(item));

    Json::Value result(Json::objectValue);
    result["contents"] = std::move(contents);
    return make_result_response(id, std::move(result));
  } catch (const std::exception& error) {
    return make_error_response(id, JSONRPC_INTERNAL_ERROR, error.what());
  } catch (...) {
    return make_error_response(
        id, JSONRPC_INTERNAL_ERROR, "Unknown resource error.");
  }
}

} // namespace howling::mcp

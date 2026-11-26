#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "services/mcp/resource_handler.h"
#include "services/mcp/tool_handler.h"
#include "json/value.h"

namespace howling::mcp {

inline constexpr std::string_view MCP_PROTOCOL_VERSION = "2025-03-26";

inline constexpr int JSONRPC_PARSE_ERROR = -32700;
inline constexpr int JSONRPC_INVALID_REQUEST = -32600;
inline constexpr int JSONRPC_METHOD_NOT_FOUND = -32601;
inline constexpr int JSONRPC_INVALID_PARAMS = -32602;
inline constexpr int JSONRPC_INTERNAL_ERROR = -32603;
inline constexpr int MCP_RESOURCE_NOT_FOUND = -32002;

/**
 * @brief Metadata reported in the MCP `initialize` response.
 */
struct server_info {
  std::string name;
  std::string version;
};

/**
 * @brief Outcome of dispatching a JSON-RPC 2.0 message or batch.
 *
 * When `response` is `std::nullopt`, no JSON-RPC response body should be sent
 * (for example, notifications, client responses, or batches containing only
 * notifications and client responses).
 */
struct dispatch_result {
  std::optional<Json::Value> response;

  /**
   * @brief Returns true when a JSON-RPC response payload should be sent.
   */
  [[nodiscard]] bool has_response() const { return response.has_value(); }
};

/**
 * @brief Registry and JSON-RPC 2.0 dispatcher for MCP `2025-03-26`.
 *
 * Thread safety: `register_tool()` and `register_resource()` are not
 * thread-safe and must complete before concurrent `dispatch()` or
 * `dispatch_request()` calls begin. Once registration is complete, `dispatch()`
 * and `dispatch_request()` are thread-safe provided that all registered
 * `tool_handler` and `resource_handler` instances are thread-safe.
 */
class mcp_dispatcher {
public:
  /**
   * @brief Constructs a dispatcher with the given server identity.
   *
   * @param info
   *  Server name and version reported during `initialize`. Both fields must be
   *  non-empty.
   */
  explicit mcp_dispatcher(server_info info);

  mcp_dispatcher(const mcp_dispatcher&) = delete;
  mcp_dispatcher& operator=(const mcp_dispatcher&) = delete;
  mcp_dispatcher(mcp_dispatcher&&) = default;
  mcp_dispatcher& operator=(mcp_dispatcher&&) = default;

  /**
   * @brief Registers a tool handler by its unique name.
   *
   * Throws `std::invalid_argument` if the handler is null, has an empty name,
   * or duplicates an already-registered tool name.
   */
  void register_tool(std::unique_ptr<tool_handler> handler);

  /**
   * @brief Registers a resource handler by its unique URI.
   *
   * Throws `std::invalid_argument` if the handler is null, has an empty URI or
   * empty name, or duplicates an already-registered resource URI.
   */
  void register_resource(std::unique_ptr<resource_handler> handler);

  /**
   * @brief Parses and dispatches a raw JSON-RPC 2.0 payload or batch array.
   */
  [[nodiscard]] dispatch_result dispatch(std::string_view raw_json) const;

  /**
   * @brief Dispatches a single parsed JSON-RPC 2.0 message object.
   */
  [[nodiscard]] dispatch_result
  dispatch_request(const Json::Value& request) const;

private:
  [[nodiscard]] Json::Value _handle_initialize() const;
  [[nodiscard]] Json::Value _handle_tools_list() const;
  [[nodiscard]] Json::Value
  _handle_tools_call(const Json::Value& id, const Json::Value& params) const;
  [[nodiscard]] Json::Value _handle_resources_list() const;
  [[nodiscard]] Json::Value _handle_resources_read(
      const Json::Value& id, const Json::Value& params) const;

  server_info _server_info;
  std::vector<std::unique_ptr<tool_handler>> _tools;
  std::unordered_map<std::string, tool_handler*> _tool_index;
  std::vector<std::unique_ptr<resource_handler>> _resources;
  std::unordered_map<std::string, resource_handler*> _resource_index;
};

} // namespace howling::mcp

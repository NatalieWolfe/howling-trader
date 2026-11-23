#pragma once

#include <condition_variable>
#include <mutex>
#include <span>
#include <utility>

#include "api/schwab.h"
#include "containers/buffered_stream.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"

namespace howling {

/**
 * @brief Streams live and prefetched candle and market data from Schwab.
 *
 * Thread safety:
 * - candle_stream() and market_stream() may be consumed concurrently from
 *   separate reader threads.
 * - Destruction is thread-safe with respect to an active start() call on
 *   another thread and waits for start() to stop before releasing buffers.
 */
class market_watch {
public:
  explicit market_watch() : _candles{1000}, _market{1000}, _schwab{} {}
  ~market_watch();

  /**
   * @brief Prefetches historical candles if enabled and streams live updates
   * until the watcher is destroyed.
   */
  void start(std::span<const stock::Symbol> symbols);

  auto candle_stream() { return _candles.stream(); }
  auto market_stream() { return _market.stream(); }

private:
  std::mutex _state_mutex;
  std::condition_variable _stopped_cv;
  bool _stopping = false;
  bool _in_start = false;
  buffered_stream<std::pair<stock::Symbol, Candle>> _candles;
  buffered_stream<Market> _market;
  schwab::stream _schwab;
};

} // namespace howling

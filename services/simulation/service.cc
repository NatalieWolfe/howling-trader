#include "services/simulation/service.h"

#include <chrono>
#include <generator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "data/load_analyzer.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "data/trade.pb.h"
#include "grpcpp/support/status.h"
#include "services/proto/simulation_service.pb.h"
#include "time/conversion.h"
#include "trading/executor.h"
#include "trading/metrics.h"
#include "trading/trading_state.h"

namespace howling {
namespace {

using std::chrono::microseconds;

grpc::Status invalid_argument(const std::string& message) {
  return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, message);
}

grpc::Status validate_request(const SimulateRequest& req) {
  if (req.symbol() == stock::Symbol::SYMBOL_UNSPECIFIED) {
    return invalid_argument("symbol not specified");
  }
  if (req.analyzer().type_case() == Analyzer::TYPE_NOT_SET) {
    return invalid_argument("analyzer.type not specified");
  }
  if (!req.has_start_at()) {
    return invalid_argument("start_at not specified");
  }
  if (!req.has_end_at()) return invalid_argument("end_at not specified");
  if (req.initial_balance() <= 0) {
    return invalid_argument("initial_balance not a positive number");
  }

  return grpc::Status::OK;
}

auto get_replay(database& db, const SimulateRequest& req) {
  return db.replay(
      req.symbol(), to_std_chrono(req.start_at()), to_std_chrono(req.end_at()));
}

} // namespace

grpc::Status simulation_service::Simulate(
    grpc::ServerContext* context,
    const SimulateRequest* request,
    SimulateResponse* response) {
  grpc::Status validation_status = validate_request(*request);
  if (!validation_status.ok()) return validation_status;

  trading_state state{
      .available_stocks = {request->symbol()},
      .account_id = "simulation",
      .initial_funds = request->initial_balance(),
      .available_funds = request->initial_balance()};
  metrics sim_metrics{
      .name = "Simulation", .initial_funds = state.initial_funds};
  executor sim_executor{state, executor::disable_real_money};

  try {
    database_pool::scoped_resource db = _db_pool.acquire();
    std::unique_ptr<analyzer> anal = load_analyzer(request->analyzer());

    for (const std::variant<Candle, Market>& record :
         get_replay(*db, *request)) {
      if (std::holds_alternative<Candle>(record)) {
        const Candle& candle = std::get<Candle>(record);
        microseconds candle_duration = to_std_chrono(candle.duration());
        state.time_now = to_std_chrono(candle.opened_at()) + candle_duration;
        add_next_minute(state.market[request->symbol()], candle);
        decision d = anal->analyze(request->symbol(), state);
        std::optional<trading_state::position> trade = d.act == action::BUY
            ? sim_executor.buy(request->symbol(), sim_metrics)
            : d.act == action::SELL
            ? sim_executor.sell(request->symbol(), sim_metrics)
            : std::nullopt;

        if (trade) {
          trading::TradeRecord& record = *response->add_trades();
          record.set_symbol(request->symbol());
          *record.mutable_executed_at() = to_proto(state.time_now);
          record.set_action(
              d.act == action::BUY ? trading::BUY : trading::SELL);
          record.set_price(trade->price);
          record.set_quantity(trade->quantity);
          record.set_confidence(d.confidence);
          record.set_dry_run(true);
        }
      } else if (std::holds_alternative<Market>(record)) {
        sim_executor.update_market(std::get<Market>(record));
      } else {
        throw std::runtime_error("Unexpected record type in replay");
      }
    }

    response->set_final_balance(state.available_funds);
  } catch (const std::exception& e) {
    return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
  } catch (...) {
    return grpc::Status(grpc::StatusCode::UNKNOWN, "Unknown error");
  }

  return grpc::Status::OK;
}

} // namespace howling

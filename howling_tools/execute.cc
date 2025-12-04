#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

#include "api/schwab.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "howling_tools/init.h"
#include "time/conversion.h"

namespace howling {
namespace {

using namespace ::std::chrono_literals;

void run() {
  schwab::stream stream;
  std::thread runner{[&]() { stream.start(); }};

  int counter = 0;
  stream.on_chart([&](stock::Symbol symbol, Candle candle) {
    std::cout << stock::Symbol_Name(symbol) << ": "
              << candle.opened_at().seconds() << "."
              << to_std_chrono(candle.duration()) << " : " << candle.open()
              << " -> " << candle.close() << " x " << candle.volume() << "\n";

    if (++counter == 1) {
      std::cout << "Stopping.\n";
      stream.stop();
    }
  });

  while (!stream.is_running()) std::this_thread::sleep_for(1ms);
  stream.add_symbol(stock::NVDA);
  runner.join();
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  howling::init(argc, argv);

  try {
    howling::run();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  } catch (...) { std::cerr << "!!!! UNKNOWN ERROR THROWN !!!!" << std::endl; }
  return 1;
}

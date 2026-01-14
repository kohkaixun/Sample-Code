#include "engine.hpp"

#include <iostream>
#include <thread>

#include "instrument_orders.hpp"
#include "io.hpp"

// std::unordered_map<uint32_t, bool> OrderBook::executed_orders;
// std::mutex OrderBook::hashmap_mut;

void Engine::accept(ClientConnection connection) {
    auto thread =
        std::thread(&Engine::connection_thread, this, std::move(connection));
    thread.detach();
}

void Engine::connection_thread(ClientConnection connection) {
    while (true) {
        ClientCommand input{};
        switch (connection.readInput(input)) {
            case ReadResult::Error:
                SyncCerr{} << "Error reading input" << std::endl;
            case ReadResult::EndOfFile:
                return;
            case ReadResult::Success:
                break;
        }

        // Functions for printing output actions in the prescribed format are
        // provided in the Output class:
        switch (input.type) {
            case input_cancel: {
                // SyncCerr{} << "Got cancel: ID: " << input.order_id << std::endl;

                // Remember to take timestamp at the appropriate time, or
                // compute an appropriate timestamp!
                // auto output_time = getCurrentTimestamp();
                // Output::OrderDeleted(input.order_id, true, output_time);

                SyncCerr() << "Cancel command" << std::endl;
                InstrumentOrders::handle_cancel_command(input);

                break;
            }

            default: {
                // SyncCerr{} << "Got order: " << static_cast<char>(input.type)
                //            << " " << input.instrument << " x " << input.count
                //            << " @ " << input.price << " ID: " << input.order_id
                //            << std::endl;

                // Remember to take timestamp at the appropriate time, or
                // compute an appropriate timestamp!
                // auto output_time = getCurrentTimestamp();
                // Output::OrderAdded(input.order_id, input.instrument,
                //                    input.price, input.count,
                //                    input.type == input_sell, output_time);
                std::unique_lock<std::mutex> instrument_lock(
                    instrument_hashmap_write_mut, std::defer_lock);
                instrument_lock.lock();
                if (!instrument_hashmap.contains(input.instrument)) {
                    instrument_hashmap.try_emplace(input.instrument);
                }
                InstrumentOrders& instrument_orders =
                    instrument_hashmap[input.instrument];
                // std::cout << "Processing command " << input.instrument <<
                // std::endl;
                instrument_lock.unlock();
                instrument_orders.process_command(input);
                break;
            }
        }

        // Additionally:

        // Remember to take timestamp at the appropriate time, or compute
        // an appropriate timestamp!
        // intmax_t output_time = getCurrentTimestamp();

        // Check the parameter names in `io.hpp`.
        // Output::OrderExecuted(123, 124, 1, 2000, 10, output_time);
    }
}

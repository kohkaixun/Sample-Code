#include "instrument_orders.hpp"

#include "engine.hpp"
#include "io.hpp"

void InstrumentOrders::process_command(ClientCommand& command) {
    // std::cout << "Processing command" << std::endl;
    if (command.type == input_buy || command.type == input_sell) {
        handle_buy_sell_command(command);
    } else {
        handle_cancel_command(command);
    }
}

void InstrumentOrders::handle_buy_sell_command(ClientCommand& command) {
    // if (!add_order_if_opp_order_book_empty(command)) {
    // std::cout << "Calling match" << std::endl;
        match(command);
    // }
}

void InstrumentOrders::handle_cancel_command(ClientCommand& command) {
    OrderBook::lockHashmapAndDeleteOrder(command.order_id);
}

// returns if opp order book is empty
// bool InstrumentOrders::add_order_if_opp_order_book_empty(ClientCommand& command) {
//     OrderBook& order_book =
//         command.type == input_buy ? buy_orderbook : sell_orderbook;
//     OrderBook& opp_order_book =
//         command.type == input_buy ? sell_orderbook : buy_orderbook;

//     std::scoped_lock lock(buy_orderbook.queue_mut, sell_orderbook.queue_mut);
//     if (opp_order_book.isOrdersQueueEmpty()) {
//         order_book.createOrder(command);
//         return true;
//     }
//     return false;    
// }

void InstrumentOrders::match(ClientCommand& command) {
    SyncCerr() << "in match command" << std::endl;
    OrderBook& orderbook =
        command.type == input_buy ? buy_orderbook : sell_orderbook;
    OrderBook& opp_orderbook =
        command.type == input_buy ? sell_orderbook : buy_orderbook;

    while (command.count > 0) {
        // bool is_opp_order_book_empty = add_order_if_opp_order_book_empty(command);
        // if (is_opp_order_book_empty) {
        //     return;
        // }

        std::unique_lock<std::mutex> opp_queue_lock(opp_orderbook.queue_mut);

        /**
         * This check may not be necessary, but in the case where the top order
         * is deleted and there are mutliple waiting active orders that can't
         * match, this will ensure some form of progress where orders are popped
         * from the queue.
         */
        // if (other_orderbook.topOrderDeleted()) {
        //     other_orderbook.resting_orders.pop();
        //     continue;
        // }

        std::unique_lock<std::mutex> hash_lock(OrderBook::hashmap_mut);
        std::optional<Order> opt_top_opp_resting_order =
            opp_orderbook.tryGetTopOrder(opp_queue_lock, hash_lock);
        hash_lock.unlock();
        Order top_opp_resting_order;
        while (true) {
            SyncCerr() << "Entered infinite while loop" << std::endl;
            while (!opt_top_opp_resting_order.has_value()) {
                opp_queue_lock.unlock();
                if (orderbook.lockQueuesAndHashAndAddOrder(command, buy_orderbook, sell_orderbook)) {
                    return;
                } else {
                    SyncCerr() << "Tried adding order " << command.order_id << " but failed" << std::endl;
                    opp_queue_lock.lock();
                    hash_lock.lock();
                    opt_top_opp_resting_order = opp_orderbook.tryGetTopOrder(opp_queue_lock, hash_lock);
                    hash_lock.unlock();
                }
            }
            SyncCerr() << "Got top opp resting order " << opt_top_opp_resting_order.value().order_id << std::endl;
            top_opp_resting_order = opt_top_opp_resting_order.value();

            if (!top_opp_resting_order.transactionable_with(command)) {
                SyncCerr() << "Order " << command.order_id << " is not transactionable with any resting orders, will try to add while loop" << std::endl;
                opt_top_opp_resting_order = std::nullopt;
                continue;
            } else {
                SyncCerr() << "Order " << command.order_id << " is transactionable, executing" << std::endl;
                break;
            }
        }
        SyncCerr() << "Order " << command.order_id << " is outside of infinite while loop" << std::endl;

        // reaches this point if top_opposing_resting_order is transactionable with active order
        // opp_queue_lock will be locked here; we also acquire hash_lock here so that top_opp_resting_order cannot be cancelled
        hash_lock.lock();
        if (command.count >= top_opp_resting_order.count) {
            SyncCerr() << "Resting order " << top_opp_resting_order.order_id << " is fully executed" << std::endl;
            if (!opp_orderbook.popTopOrder(opp_queue_lock, hash_lock)) {
                // if the top order has already been executed, then continue
                continue;
            } else {
                int64_t timestamp = getCurrentTimestamp();
                opp_queue_lock.unlock();    // unlock early if we don't need to update the top order quantity
                top_opp_resting_order.execute(command, timestamp);
            }
        } else {
            // top_opp_resting_order is a copy of the order in the heap
            // no way of editing an element in PQ in place, must pop and push from queue to update the quantity
            // top_opp_resting_order.execute(command);
            auto [transacted_qty, new_top_opp_resting_order] = top_opp_resting_order.getNewOrderAfterExecuting(command);
            if (!opp_orderbook.updateTopOrder(new_top_opp_resting_order, opp_queue_lock, hash_lock)) {
                // if the top order has already been executed, then continue
                continue;
            } else {
                int64_t timestamp = getCurrentTimestamp();
                opp_queue_lock.unlock();    // unlock early after updating top order quantity
                Order::executeCommandAfterUnlockQueueLock(command, new_top_opp_resting_order.order_id, new_top_opp_resting_order.execution_id, new_top_opp_resting_order.price, transacted_qty, timestamp);
            }
        }
    }
}


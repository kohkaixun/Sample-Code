#include "order_book.hpp"
#include "io.hpp"

std::unordered_map<uint32_t, bool> OrderBook::executed_orders;
std::mutex OrderBook::hashmap_mut;

void OrderBook::addOrderToHashMap(uint32_t order_id, std::unique_lock<std::mutex>& hash_lock) {
    if (!hash_lock.owns_lock()) {
        hash_lock.lock();
    }
    executed_orders[order_id] = false;
}

void OrderBook::updateExecutedOrderInHashMap(uint32_t order_id, std::unique_lock<std::mutex>& hash_lock) {
    if (!hash_lock.owns_lock()) {
        hash_lock.lock();
    }
    executed_orders[order_id] = true;
}

// maintains the invariant that all opposing type resting orders are never transactionable with each other
bool OrderBook::lockQueuesAndHashAndAddOrder(ClientCommand command, OrderBook& buy_orderbook, OrderBook& sell_orderbook) {
    std::lock(buy_orderbook.queue_mut, sell_orderbook.queue_mut, hashmap_mut);
    std::unique_lock<std::mutex> buy_queue_lock(buy_orderbook.queue_mut, std::adopt_lock);
    std::unique_lock<std::mutex> sell_queue_lock(sell_orderbook.queue_mut, std::adopt_lock);
    std::unique_lock<std::mutex> hash_lock(hashmap_mut, std::adopt_lock);

    OrderBook& orderbook = command.type == input_buy ? buy_orderbook : sell_orderbook;
    OrderBook& opp_orderbook = command.type == input_buy ? sell_orderbook : buy_orderbook;
    std::unique_lock<std::mutex>& opp_queue_lock = command.type == input_buy ? buy_queue_lock : sell_queue_lock;

    std::optional<Order> opp_resting_order = opp_orderbook.tryGetTopOrder(opp_queue_lock, hash_lock);

    if (opp_resting_order.has_value() && opp_resting_order.value().transactionable_with(command)) {
        return false;
    } else {
        SyncCerr() << "Adding to resting order" << std::endl;
        Order resting_order = Order(command);
        orderbook.resting_orders.push(resting_order);
        SyncCerr() << "After adding, orderbook is size " << orderbook.resting_orders.size() << ", opp orderbook is size " << opp_orderbook.resting_orders.size() << std::endl;
        Output::OrderAdded(resting_order.order_id, resting_order.instrument,
                        resting_order.price, resting_order.count,
                        resting_order.type == input_sell,
                        resting_order.timestamp);
        orderbook.addOrderToHashMap(command.order_id, hash_lock);
        return true;
    }
}

bool OrderBook::lockHashmapAndDeleteOrder(uint32_t order_id) {
    std::unique_lock<std::mutex> hash_lock(hashmap_mut);

    if (executed_orders.count(order_id) == 0 || executed_orders[order_id]) {
        Output::OrderDeleted(order_id, false, getCurrentTimestamp());
    } else {
        executed_orders[order_id] = true;
        Output::OrderDeleted(order_id, true, getCurrentTimestamp());
    }

    return true;
}

// bool OrderBook::topOrderDeleted() {
//     std::unique_lock<std::mutex> lock(hashmap_mut);
//     const Order& top_resting_order = resting_orders.top();
//     return executed_orders[top_resting_order.order_id];
// }

bool OrderBook::popTopOrder(std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock) {
   if (!queue_lock.owns_lock() || !hash_lock.owns_lock()) {
        if (queue_lock.owns_lock()) queue_lock.unlock();
        if (hash_lock.owns_lock()) hash_lock.unlock();
        queue_lock.lock();
        hash_lock.lock();
    }
    Order order = resting_orders.top();
    resting_orders.pop();
    
    if (executed_orders[order.order_id]) {
        return false;
    } else {
        updateExecutedOrderInHashMap(order.order_id, hash_lock);
    }

    return true;
}

bool OrderBook::updateTopOrder(Order top_order, std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock) {
   if (!queue_lock.owns_lock() || !hash_lock.owns_lock()) {
        if (queue_lock.owns_lock()) queue_lock.unlock();
        if (hash_lock.owns_lock()) hash_lock.unlock();
        queue_lock.lock();
        hash_lock.lock();
    }
    Order order = resting_orders.top();
    resting_orders.pop();

    if (executed_orders[order.order_id]) {
        return false;
    } else {
        resting_orders.push(top_order);
        return true;
    }
}

std::optional<Order> OrderBook::tryGetTopOrder(std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock) {
    if (!queue_lock.owns_lock() || !hash_lock.owns_lock()) {
        if (queue_lock.owns_lock()) queue_lock.unlock();
        if (hash_lock.owns_lock()) hash_lock.unlock();
        queue_lock.lock();
        hash_lock.lock();
    }

    popTillTopOrderNotEnded(queue_lock, hash_lock);

    if (resting_orders.size() > 0) {
        return resting_orders.top();
    } else {
        return std::nullopt;
    }
}

void OrderBook::popTillTopOrderNotEnded(std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock) {
    if (!queue_lock.owns_lock() || !hash_lock.owns_lock()) {
        if (queue_lock.owns_lock()) queue_lock.unlock();
        if (hash_lock.owns_lock()) hash_lock.unlock();
        queue_lock.lock();
        hash_lock.lock();
    }

    Order top_order;
    while (resting_orders.size() > 0) {
        top_order = resting_orders.top();
        if (executed_orders[top_order.order_id]) {
            resting_orders.pop();
        } else {
            break;
        }
    }
}

bool OrderBook::isOrdersQueueEmpty(std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock) {
    if (!queue_lock.owns_lock() || !hash_lock.owns_lock()) {
        if (queue_lock.owns_lock()) queue_lock.unlock();
        if (hash_lock.owns_lock()) hash_lock.unlock();
        queue_lock.lock();
        hash_lock.lock();
    }

    popTillTopOrderNotEnded(queue_lock, hash_lock);

    return resting_orders.size() == 0;
}
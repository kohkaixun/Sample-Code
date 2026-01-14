#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <cstring>
#include <queue>
#include <string>
#include <unordered_map>
#include <optional>

// #include "engine.hpp"
#include "io.hpp"

inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct Order {
    CommandType type;
    uint32_t order_id;
    uint32_t price;
    uint32_t count;
    char instrument[9];
    uint32_t execution_id;
    intmax_t timestamp;

    Order() = default;

    Order(CommandType type, uint32_t order_id, uint32_t price, uint32_t count, const char* instrument, uint32_t execution_id, intmax_t timestamp)
    : type(type),
      order_id(order_id),
      price(price),
      count(count),
      execution_id(execution_id),
      timestamp(timestamp) {
        strncpy(this->instrument, instrument, sizeof(this->instrument) - 1);
        timestamp = getCurrentTimestamp();
      }

    Order(ClientCommand active_order)
        : type(active_order.type),
          order_id(active_order.order_id),
          price(active_order.price),
          count(active_order.count),
          execution_id(0) {
        strncpy(instrument, active_order.instrument, sizeof(instrument) - 1);
        timestamp = getCurrentTimestamp();
    }

    bool transactionable_with(ClientCommand command) const {
        if (type != command.type) {
            if (type == input_buy) {
                return price >= command.price;
            } else {
                return price <= command.price;
            }
        }
        return false;
    }

    void execute(ClientCommand& command, int64_t timestamp) {
        uint32_t transacted_price = price;
        uint32_t transacted_qty = std::min(count, command.count);
        count -= transacted_qty;
        command.count -= transacted_qty;
        execution_id += 1;
        Output::OrderExecuted(order_id, command.order_id, execution_id,
                              transacted_price, transacted_qty,
                              timestamp);
    }

    // returns transacted_qty
    std::pair<uint32_t, Order> getNewOrderAfterExecuting(ClientCommand& command) {
        uint32_t transacted_qty = std::min(count, command.count);
        uint32_t new_count = count - transacted_qty;
        uint32_t new_execution_id = execution_id + 1;
        return std::pair(transacted_qty, Order(type, order_id, price, new_count, instrument, new_execution_id, timestamp));
    }

    static void executeCommandAfterUnlockQueueLock(ClientCommand& command, uint32_t order_id, uint32_t execution_id, uint32_t transacted_price, uint32_t transacted_qty, int64_t timestamp) {
        command.count -= transacted_qty;
        Output::OrderExecuted(order_id, command.order_id, execution_id,
                              transacted_price, transacted_qty,
                              timestamp);
    }

    bool operator<(const Order& other) const {
        if (type == input_buy) {
            if (price != other.price) return price < other.price;
            return timestamp > other.timestamp;
        } else {
            if (price != other.price) return price > other.price;
            return timestamp > other.timestamp;
        }
    }
};

class OrderBook {
   public:
    // to ensure no deadlocks
        // both queue mut must be acquired at once (e.g. with scoped_lock or lock);
        // and if queue and hashmap mut are both needed, hashmap_mut is always acquired after queue
    std::mutex queue_mut;
    static std::mutex hashmap_mut;
  
   private:
    std::priority_queue<Order> resting_orders;

    // ended_orders[order_id] = false means order_id is a resting order, yet to be executed fully
    // ended_order[order_id] = true means order_id has been executed fully
    static std::unordered_map<uint32_t, bool> executed_orders;

   public:
    // Below methods are not thread safe. At most, deleteOrder() and
    // topOrderDeletedCheck() lock the hash table mutex.

    static bool lockQueuesAndHashAndAddOrder(ClientCommand command, OrderBook& buy_orderbook, OrderBook& sell_orderbook);

    // NOTE: should only be called when queue mut is not locked, i.e. during a cancel command
    static bool lockHashmapAndDeleteOrder(uint32_t order_id);
    // bool topOrderDeleted();

    bool isOrdersQueueEmpty(std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock);

    std::optional<Order> tryGetTopOrder(std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock);

    bool popTopOrder(std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock);

    bool updateTopOrder(Order top_order, std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock);

  private:
    void addOrderToHashMap(uint32_t order_id, std::unique_lock<std::mutex>& hash_lock);
    void updateExecutedOrderInHashMap(uint32_t order_id, std::unique_lock<std::mutex>& hash_lock);
    void popTillTopOrderNotEnded(std::unique_lock<std::mutex>& queue_lock, std::unique_lock<std::mutex>& hash_lock);
};

#endif

#ifndef INSTRUMENT_ORDERS_HPP
#define INSTRUMENT_ORDERS_HPP

#include "order_book.hpp"

class InstrumentOrders {
    OrderBook buy_orderbook;
    OrderBook sell_orderbook;

   public:
    InstrumentOrders(): buy_orderbook(), sell_orderbook() {}
    void process_command(ClientCommand& command);
    static void handle_cancel_command(ClientCommand& command);

   private:
    void handle_buy_sell_command(ClientCommand& command);
    void match(ClientCommand& command);
};

#endif
// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include <cstring>
#include <queue>
#include <unordered_map>

#include "instrument_orders.hpp"
#include "io.hpp"

struct Engine {
   private:
    std::unordered_map<std::string, InstrumentOrders> instrument_hashmap;
    std::mutex instrument_hashmap_write_mut;

   public:
    void accept(ClientConnection conn);

   private:
    void connection_thread(ClientConnection conn);
};

#endif

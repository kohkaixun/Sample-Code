# Concurrent Order Matching Engine

This is a C++ implementation of a lock-based concurrent order matching engine.

This project was originally developed as part of a university team assignment. The code in the `matching-engine` directory represents the core matching-engine components implemented by my partner and me.

## Overview

The engine maintains a separate order book for each instrument and matches buy and sell orders according to **price-time priority**.

Each order contains:

- Order type (`Buy`, `Sell`, or `Cancel`)
- Order ID
- Price
- Quantity
- Instrument name

The implementation is designed to support concurrent processing while preserving the correctness and ordering guarantees of the matching engine. The concurrency model primarily uses fine-grained locking at the instrument and order-book level.

## Concurrency Model

The engine maintains an `InstrumentOrders` data structure for each instrument. Each `InstrumentOrders` contains two `OrderBook` instances:

- A buy order book
- A sell order book

Each `OrderBook` contains a priority queue of resting orders and is protected by its own mutex.

A hash map is used to locate the `InstrumentOrders` associated with each instrument. Access to this map is protected by a mutex. Consequently, incoming orders must briefly synchronise when locating or creating their corresponding instrument data before proceeding to the more fine-grained, per-order-book synchronisation.

This allows orders belonging to **different instruments to be processed concurrently**, because they operate on separate `InstrumentOrders` instances.

### Matching

When an active order can be matched against the most suitable resting order, the resting order is removed from the priority queue while holding the appropriate `OrderBook` mutex.

If the resting order can be completely executed, it can then be processed without continuing to hold the `OrderBook` mutex. This allows other orders to access the order book while the execution is being completed.

The active order can also continue to be modified as matches are processed, allowing multiple active orders to make progress concurrently when they operate on independent portions of the data structure.

There are two situations where concurrency must temporarily be reduced to preserve correctness.

### 1. Partial Execution of a Resting Order

If an active order has a smaller quantity than the most suitable resting order, the resting order cannot be completely executed.

Removing the resting order from the priority queue, processing it outside the mutex and reinserting the remaining quantity later could allow another thread to observe or modify the order book in a way that violates price-time priority.

Therefore, the partial execution is performed while holding the `OrderBook` mutex.

This reduces concurrency between orders accessing the same side of the same instrument, but does not prevent concurrent processing of similar instrument orders belonging to a different order type.

### 2. Converting an Active Order into a Resting Order

If an active order cannot be completely matched and must become a resting order, it needs to be inserted into the appropriate order book while maintaining the correctness of the order-book state.

This operation requires holding the mutexes for both priority queues to prevent concurrent operations from observing an inconsistent state or violating the required ordering.

Although this temporarily reduces concurrency for orders involving the same instrument, an active order can be converted into a resting order at most once during its processing. We therefore considered this synchronisation cost necessary to preserve correctness.

## Concurrency Summary

| Scenario                                 | Concurrency                                                  |
| ---------------------------------------- | ------------------------------------------------------------ |
| Orders for different instruments         | Concurrent                                                   |
| Orders accessing independent order books | Concurrent where synchronisation permits                     |
| Complete execution of a resting order    | Processing can continue after releasing the order-book mutex |
| Partial execution of a resting order     | Temporarily serialised on the relevant order book            |
| Active order converted to resting order  | Requires synchronisation across both order books     |
| Instrument lookup/creation               | Protected by a global mutex                                  |

## Design Goals

The implementation prioritises:

- Correct price-time matching under concurrent access
- Concurrent processing of orders belonging to different instruments
- Fine-grained synchronisation rather than a single global lock for all matching operations
- Maintaining consistent order-book state when orders are partially executed or converted into resting orders
- Minimising the duration for which mutexes are held where correctness permits

The concurrency design therefore represents a trade-off between parallelism and synchronisation overhead, with **correctness taking priority**.

## Limitations

The implementation uses mutex-based synchronisation and therefore does not provide unrestricted parallelism.

In particular:

- Access to the global instrument map requires synchronisation.
- Operations involving the same order book may temporarily become serialised.
- Partial executions require holding the relevant order-book mutex.
- Converting an active order into a resting order requires additional synchronisation.

These trade-offs were intentional, as maintaining correct matching behaviour and order-book consistency was prioritised over maximising concurrency.

## Background

This project was developed as part of a university team assignment. The `matching-engine` implementation was developed collaboratively with my project partner, with the concurrency and matching logic forming the core of the project.
# Concurrent Order Matching Engine
This is a C++ implementation of a lock-based concurrent matching engine.

This project was originally developed as part of a university team assignment and the code in the directory `matching-engine` represents the core matching engine components implemented by my partner and I.

## Overview

The system maintains per-instrument (items) order books and matches buy and sell orders based on price–time priority.
It is designed to operate correctly under concurrent access, with a focus on correctness.
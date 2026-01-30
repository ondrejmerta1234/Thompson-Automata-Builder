# Thompson-Automata-Builder
A C++ engine that converts regular expressions into NFAs using Thompson's construction. It handles epsilon-closure removal and provides a simulation backend to match input words against the generated automata.

## Academic Context
This project was developed as a part of a university assignment at **FIT CTU**. 
- The core logic, including NFA construction and simulation, is my original implementation.
- The `regexp` data structures and helper visualization functions in `sample.h` were provided as part of the course materials.

## Features
- **Thompson's Construction:** Converts regular expressions (Alternation, Concatenation, Iteration) into NFA fragments.
- **State Optimization:** Converts complex NFAs into cleaner versions without epsilon-transitions.
- **Match Simulator:** Uses DFS to verify words against the generated NFA.
- **Support for Special Characters:** Handles standard symbols, Epsilon (ε) and Empty set (∅).

## Technical Details
The engine works in three main stages:
1. **Build:** Recursively parses the `RegExp` variant and builds a Thompson NFA.
2. **Transform:** Processes the NFA and removes epsilon-transitions for cleaner simulation.
3. **Match:** Uses a backtracking DFS approach to explore possible paths through the automaton for a given input word.

## Disclaimer
If you are a student currently enrolled in the same course, please be aware of your university's academic integrity policy. This code is shared for portfolio purposes only.

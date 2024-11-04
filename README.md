# Advanced Compilation Project 2024

## Topic

Implementation of a static function analysis to detect potential issues in MPI collectives as a GCC plugin.

## Prerequisites

To use the plugin, you need:
- GCC 12.2.0 (Functionality with other versions is not guaranteed)
- MPI

## How to Use the Provided Plugin

```bash
make

```
To compile the plugin and the 6 test programs.

```bash
make testN
```
To compile the test program `testN` (see the `tests` folder).

```bash
make debug
```
To compile the plugin in "Debug" mode (Adds print statements in functions).

```bash
make graph
```
To generate a `.png` representation of the graph from the `.dot` files. See the `graph` folder.

```bash
make clean
```
Deletes the test executables and the plugin.

```bash
make clean_all
```
Deletes the test executables, the plugin, and any `.dot` and `.png` files related to the graphs.


Project by Hugo Lallemant & Mickaël Saes-Vincensini
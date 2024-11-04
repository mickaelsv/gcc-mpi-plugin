# Advanced Compilation Project 2024
**Static Function Analysis for MPI Collectives Detection**

![GCC Version](https://img.shields.io/badge/GCC-12.2.0-blue) ![G++ Version](https://img.shields.io/badge/G++-12.2.0-blue) ![MPI](https://img.shields.io/badge/MPI-Required-blue) ![Graphviz](https://img.shields.io/badge/Graphviz-Required-blue)

## 📄 Project Overview

This project involves implementing a static analysis plugin for detecting potential issues in MPI collectives, built as a GCC plugin.

---

## 📋 Prerequisites

To use the plugin, make sure you have:
- **GCC** version **12.2.0** (Compatibility with other versions is not guaranteed)
- **G++** version **12.2.0** (Compatibility with other versions is not guaranteed)
- **MPI** (Message Passing Interface)
- **Graphviz** with `dot` command (for generating graph images)

⚠️ In our case, this specific version of gcc had the following alias `gcc_1220 and` `g++_1220`. You should modify the Makefile.

---

## ⚙️ Installation and Setup

Clone the repository and navigate to the project directory:
```bash
git clone https://github.com/mickaelsv/gcc-mpi-plugin
cd gcc-mpi-plugin
```
Make sure 'dot' from graphviz is installed, you can run the following command 
```bash
sudo apt install graphviz  # For Debian/Ubuntu-based systems
```
Or see this page : [Github](https://github.com/graphp/graphviz)

## 🛠 Usage

### Building the Plugin and Tests
Compile the plugin and all six test programs:
```bash
make
```
To compile the plugin and the 6 test programs.

### Running Individual Tests
To compile a specific test program (e.g., testN), use:
```bash
make testN
```
*See the `tests` folder for available test programs.*
### Debug Mode
Compile the plugin in "Debug" mode (enables print statements within functions):
```bash
make debug
```

### Generating Graphs
To generate `.png` images from .dot files representing the graph:
```bash
make graph
```

## Outputs 

You can have 2 different outputs given by the plugin.

### MPI Warning
For example :
```bash
tests/test2.c: In function 'main':
tests/test2.c:26:7: warning: Potential issue: MPI collective MPI_Barrier in block 5
   26 |       MPI_Barrier(MPI_COMM_WORLD);
      |       ^~~~~~~~~~~~~~~~~~~~~~~~~~~
tests/test2.c:34:5: warning: Potential issue: MPI collective MPI_Barrier in block 7
   34 |     MPI_Barrier(MPI_COMM_WORLD);
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~~~
tests/test2.c:17:5: warning: Potential issue caused by the following fork in block 2
   17 |   if(c<10)
      |     ^
tests/test2.c:19:7: warning: Potential issue caused by the following fork in block 3
   19 |     if(c <5)
      |       ^
```

This means that there are potential issues with your MPI collectives.

## Pragma handling

For example
```bash
tests/test4.c:6:9: warning: '#pragma ProjetCA mpicoll_check' function 'main' appears multiple times
    6 | #pragma Projet_CA mpicoll_check (main, main, banane)
      |         ^~~~~~~~~
tests/test4.c:7:9: error: '#pragma ProjetCA mpicoll_check' expected parenthesis for list
    7 | #pragma Projet_CA mpicoll_check test1, test2
      |         ^~~~~~~~~
tests/test4.c:8:9: error: '#pragma ProjetCA mpicoll_check' list must be separated by commas
    8 | #pragma Projet_CA mpicoll_check (test3 test4)
      |         ^~~~~~~~~
tests/test4.c:9:9: error: '#pragma ProjetCA mpicoll_check' missing closing perenthesis
    9 | #pragma Projet_CA mpicoll_check (test5, test6
      |         ^~~~~~~~~
tests/test4.c:10:9: error: '#pragma ProjetCA mpicoll_check' unexpected closing perenthesis
   10 | #pragma Projet_CA mpicoll_check test7)
      |         ^~~~~~~~~
tests/test4.c:11:9: error: '#pragma ProjetCA mpicoll_check' argument is not a name
   11 | #pragma Projet_CA mpicoll_check ,
      |         ^~~~~~~~~
tests/test4.c:12:9: error: '#pragma ProjetCA mpicoll_check' argument is not a name
   12 | #pragma Projet_CA mpicoll_check (,
      |         ^~~~~~~~~
tests/test4.c:13:9: error: '#pragma ProjetCA mpicoll_check' argument is not a name
   13 | #pragma Projet_CA mpicoll_check (test8,)
      |         ^~~~~~~~~
tests/test4.c: In function 'main':
tests/test4.c:17:17: error: '#pragma ProjetCA mpicoll_check' pragma not allowed inside a function definition
   17 |         #pragma Projet_CA mpicoll_check main
      |                 ^~~~~~~~~
Now starting to examine function main
No potential deadlock found.
At top level:
cc1: warning: '#pragma ProjetCA mpicoll_check' function 'banane' is not declared but referenced in pragma
cc1: warning: '#pragma ProjetCA mpicoll_check' function 'test1' is not declared but referenced in pragma
cc1: warning: '#pragma ProjetCA mpicoll_check' function 'test3' is not declared but referenced in pragma
cc1: warning: '#pragma ProjetCA mpicoll_check' function 'test5' is not declared but referenced in pragma
cc1: warning: '#pragma ProjetCA mpicoll_check' function 'test6' is not declared but referenced in pragma
cc1: warning: '#pragma ProjetCA mpicoll_check' function 'test7' is not declared but referenced in pragma
cc1: warning: '#pragma ProjetCA mpicoll_check' function 'test8' is not declared but referenced in pragma
```

Pragma handling is made to understand a following logic : 
  
- #pragma Projet_CA mpicoll_check (fun1,fun2,...,funN)
- #pragma Projet_CA mpicoll_check fun

Where "fun" is a function in your code. You can use pragma to analyse only some specific functions of your code.

## Using the plugin for your programs

If you wish to use this plugin to analyse your C programs with MPI collectives, check the `Makefile`, you can either add a TARGET in it to check your code, or you can adapt your own `Makefile` by using ours.

## 🧹 Cleaning Up

Remove test executables and the plugin:
```bash
make clean
```

Remove all test executables, the plugin, and .dot and .png graph files:

```bash
make clean_all
```

## 👥 Authors

- Lallemant Hugo
- Saes-Vincensini Mickaël

## 📁 Project Structure

- `src/` - Contains the source code for the plugin.
- `tests/` - Contains test programs to validate the plugin.
- `graph/` - Contains `.dot` and generated`.png` files representing analysis graphs. 
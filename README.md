# BGPExtrapolator

A High-Performance, Cross-Compatable tool to simulate the BGP network, under Gao Rexford constraints, using real world CAIDA relationships and MRT RIB dumps. The goal of this tool is to accurately seed and propagate announcements throughout the BGP network to predict the local RIB of ASes in the network.

## How To Build

This project requires CMake 3.11 or later. If on Ubuntu 18 or older, using pip (rather than apt) to install CMake will download the latest version. For Windows, the Visual Studio CMake toolset will suffice.

Once the repository is cloned, use traditional CMake building commands in the top directory of the project

```
cmake .
make
```

The first build of the project will be longer than subsequent builds because it will download dependencies. 

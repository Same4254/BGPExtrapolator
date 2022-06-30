# BGPExtrapolator [In Development]

A High-Performance, Cross-Compatable tool to simulate the BGP network, under Gao Rexford constraints, using real world CAIDA relationships and MRT RIB dumps. The goal of this tool is to accurately seed and propagate announcements throughout the BGP network to predict the local RIB of ASes in the network.

## Extrapolator Logic

The following is a UML diagram of how the extrapolator Seeds and Propagates announcements, along with how tiebrakes are handled. Forks in the path indicate configuration options for different methods of tiebraking.

![](/ExtrapolatorVerificationLogic.png)

## How To Build

This project requires CMake 3.11 or later. If on Ubuntu 18 or older, using pip (rather than apt) to install CMake will download the latest version. For Windows, the Visual Studio CMake toolset will suffice.

Once the repository is cloned, use traditional CMake building commands in the top directory of the project.

```
./> pip install cmake
./> git clone 
./> git clone https://github.com/Same4254/BGPExtrapolator.git
./> cd BGPExtrapolator
./BGPExtrapolator/> mkdir build
./BGPExtrapolator/> cd build/
./BGPExtrapolator/build/> cmake -D CMAKE_BUILD_TYPE=RelWithDebInfo ..
./BGPExtrapolator/build/> make
./BGPExtrapolator/build/> cd ./BGPExtrapolator
./BGPExtrapolator/build/BGPExtrapolator> ./BGPExtrapolator <any command line arguments here> 
```

The first build of the project will be longer than subsequent builds because it will download dependencies and build them as well.

## Usage

The executable takes a command line argument of the file path to the json launch config file. 
```
/BGPExtrapolator/build/BGPExtrapolator> ./BGPExtrapolator --config <path to json launch file>
```

The launch file includes all of the options on how to run the extrapolator and where to put the results. Here is an example launch file that is included next to the executable:
```
{
    "Relationships": "./TestCases/RealData-Relationships.tsv",
    "Output": "./TestCases/RealData-Results.tsv",
    "Announcements": "./TestCases/RealData-Announcements.tsv",

    // NOTE: Do *NOT* use stub removal and origin only at the same time

    // Options: true, false. Default: false
    "Stub_Removal": false,

    // Options: true, false. Default: false
    "Origin_Only": false ,

    // Options: Prefer_Lowest_ASN, Random. Default: Prefer_Lowest_ASN
    "Tiebraking_Method": "Prefer_Lowest_ASN",

    // Options: Prefer_Newer, Prefer_Older, Disabled. Default: Prefer_Newer
    "Timestamp_Comparison_Method": "Prefer_Newer",

    // Options: list of ASNs to dump tracebacks of for every prefix. Empty list will dump every AS. This is the default
    "Control_Plane_Traceback_ASNs": []
}
```

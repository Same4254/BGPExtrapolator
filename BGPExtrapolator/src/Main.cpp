﻿#include "PropagationPolicies/BGPPolicy.hpp"
#include "Graphs/Graph.hpp"
#include "Testing.hpp"

#include <chrono>

/**
 * TODOs:
 *   - Multihome Policies
 *   - Code Cleanup and documentation
 *   - I broke the test cases again :)
 *   - Error detection in the mrt data / logging of some kind
 *   - Input config file
 *   - Data-Plane Traces
 *   - Program memory dump (more compact output format without traces)
 *
 *   - Stub removal optimization with origin_only propagation will break
 *
 * PERF_TODOs:
 *   - The biggest question at the moment is whether transposed local ribs will be faster for much larger datasets
 *   - Look around for other compiler flags that may help
 *   - Profile-Guided Optimization
 *   - Announcements currently store receieved from ASN, traceback requires a conversion from ASN to ID
 *      - If traceback takes longer, this can change to be an ID
 *      - However, the neighbor recieved from ID would have to lookup the ASN if doing an ASN comparison
 */
int main() {
    if (RunTestCases()) {
        std::cout << "Test Cases Passed!" << std::endl;
    } else {
        std::cout << "Test Cases Failed" << std::endl;
    }

    /*
    Graph graphWithStubs("TestCases/RealData-Relationships.txt", true);

    SeedingConfiguration config;
    config.originOnly = false;
    config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
    config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;

    graphWithStubs.SeedBlock("TestCases/RealData-Announcements.txt", config);

    auto t1 = std::chrono::high_resolution_clock::now();
    
    graphWithStubs.Propagate();

    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<float> time = t2 - t1;

    std::cout << "Propatation Time: " << time.count() << "s" << std::endl;

    std::cout << "Writing Results..." << std::endl;
    t1 = std::chrono::high_resolution_clock::now();
    //graphWithStubs.GenerateTracebackResultsCSV("TestCases/RealResults-Stubs.csv", {});
    t2 = std::chrono::high_resolution_clock::now();

    time = t2 - t1;
    std::cout << "Result Written! " << time.count() << std::endl;

    //Graph graphNoStubs("TestCases/RealData-Relationships.txt", false);

    //std::cout << "Seeding graph with stubs!" << std::endl;
    //graphWithStubs.SeedBlock("TestCases/RealData-Announcements.txt", config, 4100);
    //std::cout << "Propagating graph with stubs!" << std::endl;
    //graphWithStubs.Propagate();

    //Graph graphTest("TestCases/RealData-Relationships.txt", false);
    //std::cout << "Seeding Test Graph!" << std::endl;
    //graphTest.SeedBlock("TestCases/RealResults-Stubs.csv", config, 4100);

    //if (graphTest.CompareTo(graphWithStubs)) {
    //    std::cout << "Graphs are the same!" << std::endl;
    //} else {
    //    std::cout << "Graphs are not the same!" << std::endl;
    //}
    */
    return 0;
}

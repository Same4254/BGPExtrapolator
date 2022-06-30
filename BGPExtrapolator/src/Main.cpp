#include "PropagationPolicies/BGPPolicy.hpp"
#include "Graphs/Graph.hpp"
#include "Testing.hpp"

#include <chrono>

void Usage(bool incorrect) {
    if (incorrect)
        std::cout << "Incorrect usage, please see the correct options below." << std::endl;
    std::cout << "Usage: " << std::endl;
    std::cout << "  --help: prints the usage of the Extrapolator" << std::endl;
    std::cout << "  --config <filename>: accepts a launch configuration and performs the experiment" << std::endl;
}

/**
 * TODOs:
 *   - Multihome Policies
 *   - Code Cleanup and documentation
 *   - Error detection in the mrt data / logging of some kind
 *   - Input config file
 *   - Data-Plane Traces
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
int main(int argc, char *argv[]) {
    if (argc == 3) {
        std::string command(argv[1]);
        std::string value(argv[2]);
        if (command == "--config") {
            Graph::RunExperimentFromConfig(value);
        } else {
            Usage(true);
        }
    } else if (argc == 1) {
        if (RunTestCases()) {
            std::cout << "Test Cases Passed!" << std::endl;
        } else {
            std::cout << "Test Cases Failed" << std::endl;
            return -1;
        }

        SeedingConfiguration config;
        config.originOnly = false;
        config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
        config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;

        Graph graphWithStubs("TestCases/RealData-Relationships.tsv", false);

        std::cout << "Seeding!" << std::endl;

        auto t1 = std::chrono::high_resolution_clock::now();
        graphWithStubs.SeedBlock("TestCases/RealData-Announcements.tsv", config);
        auto t2 = std::chrono::high_resolution_clock::now();

        auto time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);
        std::cout << "Seeding Time: " << time.count() << std::endl;

        t1 = std::chrono::high_resolution_clock::now();
        
        graphWithStubs.Propagate();

        t2 = std::chrono::high_resolution_clock::now();

        time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);

        std::cout << "Propatation Time: " << time.count() << "s" << std::endl;

        //std::cout << "Writing Results..." << std::endl;
        //t1 = std::chrono::high_resolution_clock::now();
        //graphWithStubs.GenerateTracebackResultsCSV("TestCases/RealResults-Stubs.tsv", {});
        //t2 = std::chrono::high_resolution_clock::now();

        //time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);   
        //std::cout << "Result Written! " << time.count() << std::endl;

        Graph graphWithoutStubs("TestCases/RealData-Relationships.tsv", true);

        t1 = std::chrono::high_resolution_clock::now();
        graphWithoutStubs.SeedBlock("TestCases/RealData-Announcements.tsv", config);
        t2 = std::chrono::high_resolution_clock::now();

        time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);
        std::cout << "Seeding Time: " << time.count() << std::endl;

        t1 = std::chrono::high_resolution_clock::now();
        
        graphWithoutStubs.Propagate();

        t2 = std::chrono::high_resolution_clock::now();

        time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);

        std::cout << "Propatation Time: " << time.count() << "s" << std::endl;

        std::cout << CompareRibs(graphWithStubs, graphWithStubs) << std::endl;
    } else if(argc == 2) {
        std::string command(argv[1]);
        if (command == "--help")
            Usage(false);
        else
            Usage(true);
    } else {
        Usage(true);
    }

    return 0; 
}

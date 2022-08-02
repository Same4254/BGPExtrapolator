#include "Propagation_ImportPolicies/BGPDefaultImportPolicy.hpp"
#include "Graphs/Graph.hpp"
#include "Testing.hpp"

#include <chrono>

const char pathSeparator =
#ifdef _WIN32
    '\\';
#else
    '/';
#endif

void Usage(bool incorrect) {
    if (incorrect)
        std::cout << "Incorrect usage, please see the correct options below." << std::endl;
    std::cout << "Usage: " << std::endl;
    std::cout << "  --help: prints the usage of the Extrapolator" << std::endl;
    std::cout << "  --config <filename>: accepts a launch configuration and performs the experiment" << std::endl;
}

void RunExperimentFromConfig(const std::string &launchJSONPath) {
    std::ifstream launchFile(launchJSONPath);
    nlohmann::json launchJSON = nlohmann::json::parse(launchFile, nullptr, true, true);

    // File Locations
    auto rel_search = launchJSON.find("Relationships File");
    if (rel_search == launchJSON.end()) {
        std::cout << "Expected path to relationships TSV file!" << std::endl;
        return;
    }

    auto output_search = launchJSON.find("Output Folder");
    if (output_search == launchJSON.end()) {
        std::cout << "Expected path to output TSV file!" << std::endl;
        return;
    }

    auto announcements_search = launchJSON.find("Announcements File");
    if (announcements_search == launchJSON.end()) {
        std::cout << "Expected path to output TSV file!" << std::endl;
        return;
    }

    std::string relationshipsFilePath = rel_search.value();
    std::string outputFilePath = output_search.value();
    std::string announcementsFilePath = announcements_search.value();

    if (outputFilePath == "") {
        std::cout << "Output Folder cannot be an empty string!" << std::endl;
        return;
    } else if (outputFilePath.back() != pathSeparator) {
        outputFilePath += pathSeparator;
    }

    if (relationshipsFilePath == "") {
        std::cout << "Relationships file path cannot be an empty string!" << std::endl;
        return;
    }

    if (announcementsFilePath == "") {
        std::cout << "Announcements file path cannot be an empty string!" << std::endl;
        return;
    }

    // Seeding Options
    SeedingConfiguration config;

    auto seeding_config_search = launchJSON.find("Seeding Config");
    if (seeding_config_search == launchJSON.end() || !seeding_config_search.value().is_object()) {
        std::cout << "Expected a seeding configuration object!" << std::endl;
        return;
    }

    const auto seeding_config_JSONobj = seeding_config_search.value();
    auto origin_only_search = seeding_config_JSONobj.find("Origin Only");
    if (origin_only_search == seeding_config_JSONobj.end()) {
        config.originOnly = false;
    } else {
        if(!origin_only_search.value().is_boolean()) {
            std::cout << "Expected boolean value for origin only!" << std::endl;
            return;
        }

        config.originOnly = origin_only_search.value().get<bool>();
    }

    auto tiebraking_search = seeding_config_JSONobj.find("Tiebraking Method");
    if (tiebraking_search == seeding_config_JSONobj.end()) {
        config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
    } else {
        std::string method = tiebraking_search.value();
        if (method == "Prefer Lowest ASN") {
            config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
        } else if (method == "Random") {
            config.tiebrakingMethod = TIEBRAKING_METHOD::RANDOM;
        } else {
            std::cout << "Unknown tiebraking method!" << std::endl;
            return;
        }
    }

    auto timestamp_search = launchJSON.find("Timestamp Comparison Method");
    if (timestamp_search == launchJSON.end()) {
        config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;
    } else {
        std::string method = timestamp_search.value();
        if (method == "Prefer_Newer") {
            config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;
        } else if (method == "Prefer_Older") {
            config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_OLDER;
        } else if (method == "Disabled") {
            config.timestampComparison = TIMESTAMP_COMPARISON::DISABLED;
        } else {
            std::cout << "Unknown Timestamp comparison method!" << std::endl;
            return;
        }
    }

    bool stubRemoval = false;
    auto stubRemovalSearch = launchJSON.find("Stub_Removal");
    if (stubRemovalSearch != launchJSON.end()) {
        if (stubRemovalSearch.value().is_boolean()) {
            stubRemoval = stubRemovalSearch.value().get<bool>();
        } else {
            std::cout << "Unknown value for stub removal" << std::endl;
            return;
        }
    }
    
    std::vector<ASN> controlPlaneASNs;
    auto control_plane_trace_ASNs_search = launchJSON.find("Control Plane Traceback_ASNs");
    if (control_plane_trace_ASNs_search != launchJSON.end()) {
        if (control_plane_trace_ASNs_search.value().is_array()) {
            controlPlaneASNs = control_plane_trace_ASNs_search.value().get<std::vector<ASN>>();
        } else {
            std::cout << "Expected list of ASNs for control plane traceback!" << std::endl;
            return;
        }
    }

    bool dump_after_seeding = false;
    auto dump_after_seeding_search = launchJSON.find("Write Results After Seeding");
    if (dump_after_seeding_search != launchJSON.end()) {
        if (dump_after_seeding_search.value().is_boolean()) {
            dump_after_seeding = dump_after_seeding_search.value().get<bool>();
        } else {
            std::cout << "Expected a boolean for dump after seeding!" << std::endl;
            return;
        }
    }

    launchFile.close();

    Graph g(relationshipsFilePath, stubRemoval);

    std::cout << "Seeding!" << std::endl;

    auto t1 = std::chrono::high_resolution_clock::now();
    g.SeedBlock(announcementsFilePath, config);
    auto t2 = std::chrono::high_resolution_clock::now();

    auto time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);
    std::cout << "Seeding Time: " << time.count() << std::endl;

    if (dump_after_seeding) {
        t1 = std::chrono::high_resolution_clock::now();
        g.GenerateTracebackResultsCSV(outputFilePath + "Results_Seeding.tsv", controlPlaneASNs);
        t2 = std::chrono::high_resolution_clock::now();
        
        std::cout << "Writing Time: " << time.count() << "s" << std::endl;
    }

    t1 = std::chrono::high_resolution_clock::now();
    g.Propagate();
    t2 = std::chrono::high_resolution_clock::now();

    time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);

    std::cout << "Propatation Time: " << time.count() << "s" << std::endl;

    t1 = std::chrono::high_resolution_clock::now();
    g.GenerateTracebackResultsCSV(outputFilePath + "Results.tsv", controlPlaneASNs);
    t2 = std::chrono::high_resolution_clock::now();

    std::cout << "Writing Time: " << time.count() << "s" << std::endl;
}

/**
 * TODOs:
 *   - Multihome Policies
 *   - Code Cleanup and documentation
 *     - The naming of variables has gotten out of hand in some places
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
            RunExperimentFromConfig(value);
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

        Graph g("TestCases/RealData-Relationships.tsv", true);

        std::cout << "Seeding!" << std::endl;

        auto t1 = std::chrono::high_resolution_clock::now();
        g.SeedBlock("TestCases/RealData-Announcements_4000.tsv", config);
        auto t2 = std::chrono::high_resolution_clock::now();

        auto time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);
        std::cout << "Seeding Time: " << time.count() << std::endl;

        t1 = std::chrono::high_resolution_clock::now();
        
        g.Propagate();

        t2 = std::chrono::high_resolution_clock::now();

        time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);

        std::cout << "Propatation Time: " << time.count() << "s" << std::endl;

        std::cout << "Writing Results..." << std::endl;
        t1 = std::chrono::high_resolution_clock::now();
        //g.GenerateTracebackResultsCSV("TestCases/RealData-Results.tsv", {});
        t2 = std::chrono::high_resolution_clock::now();

        time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);   
        std::cout << "Result Written! " << time.count() << "s" << std::endl;

        //Graph graphWithoutStubs("TestCases/RealData-Relationships.tsv", true);

        //t1 = std::chrono::high_resolution_clock::now();
        //graphWithoutStubs.SeedBlock("TestCases/RealData-Announcements.tsv", config);
        //t2 = std::chrono::high_resolution_clock::now();

        //time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);
        //std::cout << "Seeding Time: " << time.count() << std::endl;

        //t1 = std::chrono::high_resolution_clock::now();
        //
        //graphWithoutStubs.Propagate();

        //t2 = std::chrono::high_resolution_clock::now();

        //time = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);

        //std::cout << "Propatation Time: " << time.count() << "s" << std::endl;
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

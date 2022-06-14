#include "PropagationPolicies/BGPPolicy.hpp"
#include "Graphs/Graph.hpp"

#include <chrono>

bool RunTestCases(bool stubRemoval) {
    bool testCasesPassed = true;
    std::string testCaseFolder = "TestCases/";
    std::vector<std::string> testCases = {"BGP_Prop", "BGP_Prop_Stub", "Path_len_Preference", "Relation_Preference", "Tiebrake_Preference" };
    for (auto &s : testCases) {
        //rapidcsv::Document d("TestCases/" + s + "-Relationships.txt", rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams('\t'));
        Graph g("TestCases/" + s + "-Relationships.txt", stubRemoval);

        SeedingConfiguration config;
        config.originOnly = false;
        config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
        config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;

        g.SeedBlock("TestCases/" + s + "-Announcements.txt", config, 1);

        g.Propagate();

        g.GenerateTracebackResultsCSV("TestCases/" + s + "-Results.txt", std::vector<uint32_t>());
        
        // Recreate a new graph with all of the local ribs (include the stubs if they were removed)
        Graph test("TestCases/" + s + "-Relationships.txt", false);
        test.SeedBlock("TestCases/" + s + "-Results.txt", config, 1);

        //rapidcsv::Document d2("TestCases/" + s + "-Relationships.txt", rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams('\t'));
        Graph g2("TestCases/" + s + "-Relationships.txt", false);
        g2.SeedBlock("TestCases/" + s + "-Truth.txt", config, 1);

        if(!test.CompareTo(g2)) {
            std::cout << "Test Case " + s + " Failed!" << std::endl;
            testCasesPassed = false;
        }
    }

    return testCasesPassed;
}

int main() {
    /*if (RunTestCases(true) && RunTestCases(false)) {
        std::cout << "Test Cases Passed!" << std::endl;
    } else {
        std::cout << "Test Cases Failed" << std::endl;
    }*/

    Graph graphWithStubs("TestCases/RealData-Relationships.txt", false);

    SeedingConfiguration config;
    config.originOnly = false;
    config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
    config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;

    graphWithStubs.SeedBlock("TestCases/RealData-Announcements.txt", config, 4100);

    auto t1 = std::chrono::high_resolution_clock::now();
    
    graphWithStubs.Propagate();

    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<float> time = t2 - t1;

    std::cout << "Propatation Time: " << time.count() << "s" << std::endl;

    std::cout << "Writing Results..." << std::endl;
    t1 = std::chrono::high_resolution_clock::now();
    graphWithStubs.GenerateTracebackResultsCSV("TestCases/RealResults-Stubs.csv", {});
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

    return 0;
}

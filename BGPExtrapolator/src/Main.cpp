#include "PropagationPolicies/BGPPolicy.hpp"
#include "Graphs/Graph.hpp"

int main() {
    bool testCasesPassed = true;
    std::string testCaseFolder = "TestCases/";
    std::vector<std::string> testCases = {"BGP_Prop", "Path_len_Preference", "Relation_Preference", "Tiebrake_Preference" };
    for (auto &s : testCases) {
        rapidcsv::Document d("TestCases/" + s + "-Relationships.txt", rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams('\t'));
        Graph g(d, 1, 1);

        SeedingConfiguration config;
        config.originOnly = false;
        config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
        config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;

        g.SeedBlock("TestCases/" + s + "-Announcements.txt", config);

        g.Propagate();

        g.GenerateResultsCSV("TestCases/" + s + "-Results.txt", std::vector<uint32_t>());

        rapidcsv::Document d2("TestCases/" + s + "-Relationships.txt", rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams('\t'));
        Graph g2(d2, 1, 100);
        g2.SeedBlock("TestCases/" + s + "-Truth.txt", config);

        if(!g.CompareTo(g2)) {
            std::cout << "Test Case " + s + " Failed!" << std::endl;
            testCasesPassed = false;
        }
    }

    if (testCasesPassed) {
        std::cout << "Test Cases Passed!" << std::endl;
    } else {
        std::cout << "Test Cases Failed" << std::endl;
    }

	return 0;
}

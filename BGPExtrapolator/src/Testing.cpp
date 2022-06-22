#include "Testing.hpp"

bool CompareTraceOutputTSVs(const std::string &filePath1, const std::string &filePath2) {
    std::set<std::vector<ASN>> d1Paths, d2Paths;

    rapidcsv::Document d1(filePath1, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

    int traceRowIndex = 0;
    for (int i = 0; i < d1.GetRowCount(); i++) {
        std::string value = d1.GetCell<std::string>("index", i);
        if(value == "static_index") {
            traceRowIndex = i;
            break;
        }
    }

    rapidcsv::Document d1_trace(filePath1, rapidcsv::LabelParams(traceRowIndex + 1, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));
    for (int i = 0; i < d1_trace.GetRowCount(); i++) {
        std::string as_path_string = d1_trace.GetCell<std::string>("as_path", i);
        d1Paths.insert(Util::parseASNList(as_path_string));
    }

    rapidcsv::Document d2(filePath2, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

    traceRowIndex = 0;
    for (int i = 0; i < d2.GetRowCount(); i++) {
        std::string value = d2.GetCell<std::string>("index", i);
        if(value == "static_index") {
            traceRowIndex = i;
            break;
        }
    }

    rapidcsv::Document d2_trace(filePath2, rapidcsv::LabelParams(traceRowIndex + 1, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));
    for (int i = 0; i < d2_trace.GetRowCount(); i++) {
        std::string as_path_string = d2_trace.GetCell<std::string>("as_path", i);
        d2Paths.insert(Util::parseASNList(as_path_string));
    }

    if (d1Paths != d2Paths)
        return false;
    return true;
}

bool RunPropagationTestCases(bool stubRemoval) {
    bool testCasesPassed = true;
    std::string testCaseFolder = "TestCases/";
    std::vector<std::string> testCases = {"BGP_Prop", "Path_len_Preference", "Relation_Preference", "Tiebrake_Preference" };
    for (auto &s : testCases) {
        //rapidcsv::Document d("TestCases/" + s + "-Relationships.txt", rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams('\t'));
        Graph g("TestCases/" + s + "-Relationships.tsv", stubRemoval);

        SeedingConfiguration config;
        config.originOnly = false;
        config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
        config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;

        g.SeedBlock("TestCases/" + s + "-Announcements.tsv", config);

        g.Propagate();

        std::string outputFileName = "TestCases/" + s + "-Results" + (stubRemoval ? "_StubsRemoval" : "") + ".tsv";
        g.GenerateTracebackResultsCSV(outputFileName, std::vector<uint32_t>());

        if (!CompareTraceOutputTSVs(outputFileName, "TestCases/" + s + "-Truth.tsv"))
            testCasesPassed = false;
    }

    return testCasesPassed;
}

bool RunTestCases() {
    return RunPropagationTestCases(true) && RunPropagationTestCases(false);
}

#include "Testing.hpp"

std::vector<std::string> propagationTestNames = {"BGP_Prop", "Path_len_Preference", "Relation_Preference", "Tiebrake_Preference" };

void GenerateTestCaseStateResultFiles(bool stubRemoval) {
    std::cout << "NOTE: Generating save states to the test case folder ( stubRemoval: " << stubRemoval << " ). Make sure you intended to do this!" << std::endl;

    for (auto &s : propagationTestNames) {
        Graph g("TestCases/" + s + "-Relationships.tsv", stubRemoval);
    
        SeedingConfiguration config;
        config.originOnly = false;
        config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
        config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;

        g.SeedBlock("TestCases/" + s + "-Announcements.tsv", config);
        g.DumpState("TestCases/" + s + "-SeedState" + (stubRemoval ? "_StubsRemoval" : "") + "-Truth");

        g.Propagate();
        g.DumpState("TestCases/" + s + "-PropagationState" + (stubRemoval ? "_StubsRemoval" : "") + "-Truth");

        //if (!stubRemoval)
            //g.GenerateTracebackResultsCSV("TestCases/" + s + "-Results-Truth.tsv", {});
    }
}

bool RunPropagationTestCases(bool stubRemoval) {
    bool testCasesPassed = true;
    std::string testCaseFolder = "TestCases/";
    for (auto &s : propagationTestNames) {
        Graph g("TestCases/" + s + "-Relationships.tsv", stubRemoval);

        SeedingConfiguration config;
        config.originOnly = false;
        config.tiebrakingMethod = TIEBRAKING_METHOD::PREFER_LOWEST_ASN;
        config.timestampComparison = TIMESTAMP_COMPARISON::PREFER_NEWER;

        g.SeedBlock("TestCases/" + s + "-Announcements.tsv", config);
        Graph seedCompare("TestCases/" + s + "-SeedState" + (stubRemoval ? "_StubsRemoval" : "") + "-Truth");

        if (!CompareRibs(g, seedCompare)) {
            std::cout << s + " Local Ribs are not the same after seeding" << std::endl;
            testCasesPassed = false;
        }

        g.Propagate();
        Graph propagationCompare("TestCases/" + s + "-PropagationState" + (stubRemoval ? "_StubsRemoval" : "") + "-Truth");

        if (!CompareRibs(g, propagationCompare)) {
            std::cout << s + " Local Ribs are not the same after propagation" << std::endl;
            testCasesPassed = false;
        }

        std::string outputFileName = "TestCases/" + s + "-Results" + (stubRemoval ? "_StubsRemoval" : "") + ".tsv";
        g.GenerateTracebackResultsCSV(outputFileName, std::vector<uint32_t>());

        if (!CompareTraceOutputCSVs(outputFileName, "TestCases/" + s + "-Results-Truth.tsv")) {
            std::cout << s + " Output file not the same!" << std::endl;

            testCasesPassed = false;
        }
    }

    return testCasesPassed;
}

bool CompareTraceOutputCSVs(const std::string &filePath1, const std::string &filePath2) {
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

bool CommonNonStubASNs(const Graph &graph1, const Graph &graph2, std::set<ASN> &asns) {
    asns.clear();

    for (size_t i = 0; i < graph1.GetNumASes(); i++) {
        ASN asn = graph1.GetASN(i);
        if (graph2.ContainsASN(asn)) {
            asns.insert(asn);
        } else if (graph2.IsStub(asn)) {
            continue;
        } else {
            return false;
        }
    }

    for (size_t i = 0; i < graph2.GetNumASes(); i++) {
        ASN asn = graph2.GetASN(i);
        if (graph1.ContainsASN(asn)) {
            asns.insert(asn);
        } else if (graph1.IsStub(asn)) {
            continue;
        } else {
            return false;
        }
    }

    return true;
}

bool CompareRibs(const Graph &graph1, const Graph &graph2) {
    if (graph1.GetNumPrefixes() != graph2.GetNumPrefixes())
        return false;

    std::set<ASN> asnsToCompare;
    if (!CommonNonStubASNs(graph1, graph2, asnsToCompare))
        return false;

    for (auto asn : asnsToCompare) {
        ASN_ID id1 = graph1.GetASNID(asn);
        ASN_ID id2 = graph2.GetASNID(asn);
       
        for (size_t prefix_id = 0; prefix_id < graph1.GetNumPrefixes(); prefix_id++) {
            const AnnouncementCachedData &ann1 = graph1.GetCachedData_ReadOnly(id1, prefix_id);
            const AnnouncementCachedData &ann2 = graph2.GetCachedData_ReadOnly(id2, prefix_id);

            if (ann1.pathLength != ann2.pathLength)
                return false;
           
            if (ann1.pathLength == 0)
                continue;

            if (ann1.recievedFromASN != ann2.recievedFromASN || ann1.relationship != ann2.relationship)
                return false;

            const AnnouncementStaticData &staticData1 = graph1.GetStaticData_ReadOnly(ann1.staticDataIndex);
            const AnnouncementStaticData &staticData2 = graph2.GetStaticData_ReadOnly(ann2.staticDataIndex);

            if (staticData1.timestamp != staticData2.timestamp || staticData1.origin != staticData2.origin || staticData1.prefixString != staticData2.prefixString)
                return false;
        }
    }

    return true;
}

bool RunTestCases() {
    // Uncomment to save the current code as the "correct" version
    //GenerateTestCaseStateResultFiles(false);
    //GenerateTestCaseStateResultFiles(true);

    return RunPropagationTestCases(true) && RunPropagationTestCases(false);
}

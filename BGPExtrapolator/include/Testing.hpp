#pragma once

#include "Graphs/Graph.hpp"

#include <set>

extern void GenerateTestCaseStateResultFiles(bool stubRemoval);

extern bool CompareTraceOutputCSVs(const std::string &filePath1, const std::string &filePath2);
extern bool CompareRibs(const Graph &graph1, const Graph &graph2);
extern bool RunTestCases();

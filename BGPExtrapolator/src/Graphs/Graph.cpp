#include "Graphs/Graph.hpp"

#include "PropagationPolicies/BGPPolicy.hpp"

//Temporary struct for building the ranks
struct RelationshipInfo {
    ASN asn;
    ASN_ID asnID;

    int rank;
    std::vector<ASN> peers, customers, providers;
};

Graph::Graph(rapidcsv::Document& relationshipsCSV, size_t maximumPrefixBlockID, size_t maximumSeededAnnouncements) : localRibs(relationshipsCSV.GetRowCount(), maximumPrefixBlockID) {
    //***** Relationship Parsing *****//
    idToPolicy.resize(relationshipsCSV.GetRowCount());
    asIDToProviderIDs.resize(relationshipsCSV.GetRowCount());
    asIDToPeerIDs.resize(relationshipsCSV.GetRowCount());
    asIDToCustomerIDs.resize(relationshipsCSV.GetRowCount());

    std::vector<RelationshipInfo> relationshipInfo;
    relationshipInfo.resize(relationshipsCSV.GetRowCount());

    size_t maximumRank = 0;
    ASN_ID nextID = 0;

    //Store the relationships, assign IDs, and find the maximum rank
    for (size_t rowIndex = 0; rowIndex < relationshipsCSV.GetRowCount(); rowIndex++) {
        RelationshipInfo& info = relationshipInfo[nextID];

        info.asn = relationshipsCSV.GetCell<ASN>("asn", rowIndex);
        info.asnID = nextID;

        asnToID.insert({ info.asn, info.asnID });

        info.rank = relationshipsCSV.GetCell<int>("propagation_rank", rowIndex);

        if (info.rank > maximumRank)
            maximumRank = info.rank;

        info.providers = Util::parseASNList(relationshipsCSV.GetCell<std::string>("providers", rowIndex));
        info.peers = Util::parseASNList(relationshipsCSV.GetCell<std::string>("peers", rowIndex));
        info.customers = Util::parseASNList(relationshipsCSV.GetCell<std::string>("customers", rowIndex));

        //PERF_TODO: These can be optimized (redundant inserts)
        for (auto providerASN : info.providers) {
            relationshipPriority.insert({ std::make_pair(info.asn, providerASN), RELATIONSHIP_PRIORITY_CUSTOMER_TO_PROVIDER });
            relationshipPriority.insert({ std::make_pair(providerASN, info.asn), RELATIONSHIP_PRIORITY_PROVIDER_TO_CUSTOMER });
        }

        for (auto peerASN : info.peers) {
            relationshipPriority.insert({ std::make_pair(info.asn, peerASN), RELATIONSHIP_PRIORITY_PEER_TO_PEER });
            relationshipPriority.insert({ std::make_pair(peerASN, info.asn), RELATIONSHIP_PRIORITY_PEER_TO_PEER });
        }

        for (auto cutomerASN : info.customers) {
            relationshipPriority.insert({ std::make_pair(info.asn, cutomerASN), RELATIONSHIP_PRIORITY_PROVIDER_TO_CUSTOMER });
            relationshipPriority.insert({ std::make_pair(cutomerASN, info.asn), RELATIONSHIP_PRIORITY_CUSTOMER_TO_PROVIDER });
        }

        //idToPolicy[nextID] = new BGPPolicy<>(info.asn, info.asnID);
        idToPolicy[nextID] = new BGPPolicy<BGPPolicy<>::CompareRelationships, BGPPolicy<>::ComparePathLengths, BGPPolicy<>::CompareTimestampsPreferNew, BGPPolicy<>::CompareASNsPreferSmaller>(info.asn, info.asnID);

        nextID++;
    }

    // Allocate space for the rank structure and point its content to the corresponding data
    // Also put the pointer to other AS data in relationship structures 

    //ranks are 0 indexed, so the size of the structure holding the ranks is 1 + maximum index
    rankToIDs.resize(maximumRank + 1);
    for (int i = 0; i < relationshipInfo.size(); i++) {
        RelationshipInfo& info = relationshipInfo[i];

        rankToIDs[relationshipInfo[i].rank].push_back(info.asnID);

        for (ASN provider : info.providers) {
            auto idSearch = asnToID.find(provider);
            if (idSearch == asnToID.end())
                continue;

            asIDToProviderIDs[i].push_back(idSearch->second);
        }

        for (ASN peer : info.peers) {
            auto idSearch = asnToID.find(peer);
            if (idSearch == asnToID.end())
                continue;

            asIDToPeerIDs[i].push_back(idSearch->second);
        }

        for (ASN customer : info.customers) {
            auto idSearch = asnToID.find(customer);
            if (idSearch == asnToID.end())
                continue;

            asIDToCustomerIDs[i].push_back(idSearch->second);
        }
    }

    announcementStaticData.resize(maximumSeededAnnouncements);

    ResetAllAnnouncements();
}

Graph::~Graph() {
    for (PropagationPolicy* policy : idToPolicy)
        delete policy;
}

bool Graph::CompareTo(Graph &graph2) {
    if(localRibs.numAses != graph2.localRibs.numAses || localRibs.numPrefixes != graph2.localRibs.numPrefixes)
        return false;
    
    for (ASN_ID asn_id = 0; asn_id < localRibs.numAses; asn_id++) {
    for (size_t prefix_id = 0; prefix_id < localRibs.numPrefixes; prefix_id++) {
        AnnouncementCachedData &ann1 = localRibs.GetAnnouncement(asn_id, prefix_id);
        AnnouncementCachedData &ann2 = localRibs.GetAnnouncement(asn_id, prefix_id);

        if (ann1.pathLength != ann2.pathLength)
            return false;
       
        if (ann1.pathLength == 0)
            continue;

        if (ann1.recievedFromASN != ann2.recievedFromASN || ann1.relationship != ann2.relationship)
            return false;

        const AnnouncementStaticData &staticData1 = GetStaticData(ann1.staticDataIndex);
        const AnnouncementStaticData &staticData2 = graph2.GetStaticData(ann2.staticDataIndex);

        if (staticData1.timestamp != staticData2.timestamp || staticData1.origin != staticData2.origin || staticData1.prefixString != staticData2.prefixString)
            return false;
    }}

    return true;
}

void Graph::ResetAllAnnouncements() {
    for (int i = 0; i < localRibs.numAses; i++) {
        for (int j = 0; j < localRibs.numPrefixes; j++) {
            AnnouncementCachedData& ann = localRibs.GetAnnouncement(i, j);
            ann.pathLength = 0;
            ann.recievedFromASN = 0;
            ann.seeded = 0;
            //ann.staticData = nullptr;
            ann.staticDataIndex = 0;
        }
    }
}

void Graph::ResetAllNonSeededAnnouncements() {
    for (int i = 0; i < localRibs.numAses; i++) {
        for (int j = 0; j < localRibs.numPrefixes; j++) {
            AnnouncementCachedData& ann = localRibs.GetAnnouncement(i, j);

            if (ann.seeded)
                continue;

            ann.pathLength = 0;
            ann.recievedFromASN = 0;
            ann.seeded = 0;
            //ann.staticData = nullptr;
            ann.staticDataIndex = 0;
        }
    }
}

void Graph::SeedBlock(const std::string& filePathAnnouncements, const SeedingConfiguration &config) {
    rapidcsv::Document announcements_csv(filePathAnnouncements, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

    for (size_t row_index = 0; row_index < announcements_csv.GetRowCount(); row_index++) {
        //***** PARSING
        std::string prefixString = announcements_csv.GetCell<std::string>("prefix", row_index);
        std::string as_path_string = announcements_csv.GetCell<std::string>("as_path", row_index);

        std::vector<ASN> as_path = Util::parseASNList(as_path_string);

        int64_t timestamp = announcements_csv.GetCell<int64_t>("timestamp", row_index);
        ASN origin = announcements_csv.GetCell<ASN>("origin", row_index);

        uint32_t prefix_id = announcements_csv.GetCell<uint32_t>("prefix_id", row_index);
        uint32_t prefix_block_id = announcements_csv.GetCell<uint32_t>("prefix_block_id", row_index);

        Prefix prefix;
        prefix.global_id = prefix_id;
        prefix.block_id = prefix_block_id;

        SeedPath(as_path, row_index, prefix, prefixString, timestamp, config);
    }
}

//TODO Recieved_from needs to be much more robust to the absence of known ASNs in the graph.
void Graph::SeedPath(const std::vector<ASN>& asPath, size_t staticDataIndex, const Prefix& prefix, const std::string& prefixString, int64_t timestamp, const SeedingConfiguration &config) {
    if (asPath.size() == 0)
        return;

    AnnouncementStaticData &staticData = announcementStaticData[staticDataIndex];

    staticData.origin = asPath[asPath.size() - 1];
    staticData.prefix = prefix;
    staticData.timestamp = timestamp;
    staticData.prefixString = prefixString;

    ASN_ID lastID = 0;
    bool lastIDSet = false;

    int end_index = config.originOnly ? asPath.size() - 1 : 0;
    for (int i = asPath.size() - 1; i >= end_index; i--) {
        // If AS not in the graph, skip it
        // TODO: This should be an error
        auto asn_search = asnToID.find(asPath[i]);
        if (asn_search == asnToID.end())
            continue;

        //If there is prepending, then just keep going along the path. The length is accounted for.
        if (i < asPath.size() - 1 && asPath[i] == asPath[i + 1])
            continue;

        ASN asn = asn_search->first;
        ASN_ID asn_id = asn_search->second;

        uint8_t relationship = RELATIONSHIP_PRIORITY_ORIGIN;
        if (i < asPath.size() - 1) {
            auto search = relationshipPriority.find(std::make_pair(asPath[i + 1], asn));
            if (search == relationshipPriority.end()) {
                //TODO check for stub: https://github.com/c-morris/BGPExtrapolator/commit/364abb3d70d8e6aa752450e756348b2e1f82c739
                relationship = RELATIONSHIP_PRIORITY_BROKEN;
            } else {
                relationship = search->second;
            }
        }

        uint8_t newPathLength = asPath.size() - i;

        //TODO: this needs to be fixed
        ASN_ID recieved_from_id = asn_id;
        if (i < asPath.size() - 1) {
            auto previous_search = asnToID.find(asPath[i + 1]);
            if (previous_search == asnToID.end() && lastIDSet) {
                recieved_from_id = lastIDSet;
            } else if (previous_search != asnToID.end()) {
                recieved_from_id = previous_search->second;
            }
        }

        lastIDSet = true;
        lastID = asn_id;

        //TODO: Not all of these if-statements plz
        // 
        //If there exists an announcement for this prefix already
        AnnouncementCachedData& currentAnn = localRibs.GetAnnouncement(asn_id, prefix.block_id);

        //if (currentAnn.staticData != nullptr) {
        if (currentAnn.pathLength != 0) {
            int64_t currentTimestamp = announcementStaticData[currentAnn.staticDataIndex].timestamp;

            if (config.timestampComparison == TIMESTAMP_COMPARISON::PREFER_NEWER && timestamp > currentTimestamp)
                continue;
            else if (config.timestampComparison == TIMESTAMP_COMPARISON::PREFER_OLDER && timestamp < currentTimestamp)
                continue;

            //if (timestamp == currentAnn.staticData->timestamp) {
            if (timestamp == currentTimestamp) {
                //if (recieving_as.loc_rib[prefix.block_id].priority.allFields > priority.allFields)
                //  continue;

                if (currentAnn.relationship > relationship || currentAnn.pathLength < newPathLength)
                    continue;

                if (currentAnn.relationship == relationship && currentAnn.pathLength == newPathLength) {
                    if (config.tiebrakingMethod == TIEBRAKING_METHOD::RANDOM) {
                        if (rand() % 2 == 0)
                            continue;
                    } else {//lowest recieved_from ASN wins
                        if (currentAnn.recievedFromASN < asPath[i + 1])
                            continue;
                    }
                }
            }
        }

        //Recieve from itself if it is the origin
        ASN recieved_from_asn = i < asPath.size() - 1 ? asPath[i + 1] : asn;

        //accept the announcement
        //recieving_as.loc_rib[prefix.block_id].fill(recieved_from_asn, priority, staticData);
        currentAnn.pathLength = newPathLength;
        currentAnn.recievedFromASN = recieved_from_asn;
        currentAnn.relationship = relationship;
        currentAnn.seeded = 1;
        //currentAnn.staticData = &staticData;
        currentAnn.staticDataIndex = staticDataIndex;
    }
}

void Graph::Propagate() {
    // ************ Propagate Up ************//

    // start at the second rank because the first has no customers
    for (size_t i = 1; i < rankToIDs.size(); i++)
        for (auto& providerID : rankToIDs[i])
            idToPolicy[providerID]->ProcessCustomerAnnouncements(*this, asIDToCustomerIDs[providerID]);

    for (size_t i = 0; i < rankToIDs.size(); i++)
        for (auto& asID : rankToIDs[i])
            idToPolicy[asID]->ProcessPeerAnnouncements(*this, asIDToPeerIDs[asID]);

    // ************ Propagate Down ************//
    //Customer looks up to the provider and looks at its data, that is why the - 2 is there
    for (int i = rankToIDs.size() - 2; i >= 0; i--)
        for (auto& customerID : rankToIDs[i])
            idToPolicy[customerID]->ProcessProviderAnnouncements(*this, asIDToProviderIDs[customerID]);
}

std::vector<ASN> Graph::Traceback(const ASN& startingASN, const uint32_t& prefixBlockID) {
    std::vector<ASN> as_path;

    ASN_ID asnID = asnToID[startingASN];
    ASN asn = startingASN;

    //origin recieves from itself
    while (true) {
        AnnouncementCachedData& ann = localRibs.GetAnnouncement(asnID, prefixBlockID);
        as_path.push_back(asn);

        if (asn == ann.recievedFromASN)
            break;

        asn = ann.recievedFromASN;
        asnID = asnToID.at(asn);
    }

    return as_path;
}

void Graph::GenerateResultsCSV(const std::string& resultsFilePath, std::vector<ASN> localRibsToDump) {
    //Create the file, delete if it exists already (std::fstream::trunc)
    std::fstream fStream(resultsFilePath, std::fstream::in | std::fstream::out | std::fstream::trunc);
    rapidcsv::Document document("", rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER, false, false));

    //Wow, rapidcsv kinda not doing so hot here...
    document.InsertColumn<std::string>(document.GetColumnCount(), std::vector<std::string>(), "prefix");
    document.InsertColumn<std::string>(document.GetColumnCount(), std::vector<std::string>(), "as_path");
    document.InsertColumn<std::int64_t>(document.GetColumnCount(), std::vector<int64_t>(), "timestamp");
    document.InsertColumn<std::ASN>(document.GetColumnCount(), std::vector<ASN>(), "origin");
    document.InsertColumn<uint32_t>(document.GetColumnCount(), std::vector<uint32_t>(), "prefix_id");
    document.InsertColumn<uint32_t>(document.GetColumnCount(), std::vector<uint32_t>(), "block_id");
    document.InsertColumn<uint32_t>(document.GetColumnCount(), std::vector<uint32_t>(), "prefix_block_id");

    size_t prefix_column_index = document.GetColumnIdx("prefix");
    size_t as_path_column_index = document.GetColumnIdx("as_path");
    size_t timestamp_column_index = document.GetColumnIdx("timestamp");
    size_t origin_column_index = document.GetColumnIdx("origin");
    size_t prefix_id_column_index = document.GetColumnIdx("prefix_id");
    size_t block_id_column_index = document.GetColumnIdx("block_id");
    size_t prefix_block_id_index = document.GetColumnIdx("prefix_block_id");
    
    if (localRibsToDump.empty()) {
        for (const auto& kv : asnToID)
            localRibsToDump.push_back(kv.first);
    }

    //Only dump the RIB of ASes we care about.
    for (ASN asn : localRibsToDump) {
        auto id_search = asnToID.find(asn);
        if (id_search == asnToID.end())
            continue;

        ASN_ID id = id_search->second;

        for (uint32_t prefixBlockID = 0; prefixBlockID < localRibs.numPrefixes; prefixBlockID++) {
            const AnnouncementCachedData &ann = localRibs.GetAnnouncement(id, prefixBlockID);

            //Do nothing if there is no actual announcement at the prefix
            //if (localRibs.GetAnnouncement(id, prefixBlockID).staticData == nullptr)
            if (localRibs.GetAnnouncement(id, prefixBlockID).pathLength == 0)
                continue;

            std::vector<ASN> as_path = Traceback(asn, prefixBlockID);

            //***** Build String
            std::stringstream string_stream;

            string_stream << "{";
            for (size_t j = 0; j < as_path.size(); j++) {
                if (j == as_path.size() - 1)
                    string_stream << as_path[j];
                else
                    string_stream << as_path[j] << ",";
            }

            string_stream << "}";

            //****** Write to CSV
            document.InsertRow<int>(0);
            size_t row_index = 0;

            AnnouncementStaticData& staticData = announcementStaticData[ann.staticDataIndex];

            //document.SetCell<std::string>(prefix_column_index, row_index, ann.staticData->prefixString);

            document.SetCell<std::string>(prefix_column_index, row_index, staticData.prefixString);
            document.SetCell<std::string>(as_path_column_index, row_index, string_stream.str());
            document.SetCell<int64_t>(timestamp_column_index, row_index, staticData.timestamp);
            document.SetCell<ASN>(origin_column_index, row_index, staticData.origin);
            document.SetCell<uint32_t>(prefix_id_column_index, row_index, staticData.prefix.global_id);
            document.SetCell<uint32_t>(block_id_column_index, row_index, 0);
            document.SetCell<uint32_t>(prefix_block_id_index, row_index, prefixBlockID);

            //document.SetCell<int64_t>(timestamp_column_index, row_index, ann.staticData->timestamp);
            //document.SetCell<ASN>(origin_column_index, row_index, ann.staticData->origin);
            //document.SetCell<uint32_t>(prefix_id_column_index, row_index, ann.staticData->prefix.global_id);
        }
    }

    document.Save(fStream);
    fStream.close();
}

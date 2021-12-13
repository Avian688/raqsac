#include "CentralSchedulerRaqsac.h"

#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include <fstream>

using namespace std;
using namespace std::chrono;

namespace inet {

Define_Module(CentralSchedulerRaqsac);
#define PKT_SIZE 1500
CentralSchedulerRaqsac::~CentralSchedulerRaqsac()
{
    cancelAndDelete(startManagerNode);
    cancelAndDelete(stopSimulation);

}

void CentralSchedulerRaqsac::initialize(int stage)
{
    isWebSearchWorkLoad = par("isWebSearchWorkLoad");
    indexWorkLoad = 0;
    // Record start time
    t1 = high_resolution_clock::now();
//    omnetpp::envir::Stopwatch stopwatch;
//    stopwatch.setRealTimeLimit(5);
    multicastGrpPortNum = 1;
    numCompletedShortFlows = par("numCompletedShortFlows");
    WATCH(numCompletedShortFlows);
    numCompletedLongFlows = par("numCompletedShortFlows");
    IW = par("IW");
    switchQueueLength = par("switchQueueLength");
    randMapShortFlowsVec.setName("randMapShortFlowsVec");
    permMapShortFlowsVector.setName("permMapShortFlowsVector");

    permMapLongFlowsVec.setName("permMapLongFlowsVec");
    permMapShortFlowsVec.setName("permMapShortFlowsVec");
    numRaqsacSessionAppsVec.setName("numRaqsacSessionAppsVec");
    numRaqsacSinkAppsVec.setName("numRaqsacSinkAppsVec");
    matSrc.setName("matSrc");
    matDest.setName("matDest");
    nodes.setName("nodes");

    std::cout << "\n\n Central flow scheduler \n";
    kValue = par("kValue");
    trafficMatrixType = par("trafficMatrixType");
    arrivalRate = par("arrivalRate");

    perFlowEcmp = par("perFlowEcmp");
    perPacketEcmp = par("perPacketEcmp");

    // seed vale and rng
    seedValue = par("seedValue");
    srand(seedValue);
    PRNG = std::mt19937(seedValue);
    // note arrivalRate should be called before this line
    expDistribution = std::exponential_distribution<double>(arrivalRate);
    expDistributionForRqDecdoing = std::exponential_distribution<double>(2);

    flowSize = par("flowSize");
    numShortFlows = par("numShortFlows");
    longFlowSize = par("longFlowSize");
    numServers = std::pow(kValue, 3) / 4;
    shuffle = par("shuffle");
    randomGroup = par("randomGroup");

    numRunningShortFlowsNow = par("numRunningShortFlowsNow");
    percentLongFlowNodes = par("percentLongFlowNodes");
    oneToOne = par("oneToOne");

    numTimesDecodingFailed = par("numTimesDecodingFailed");
    numTimesDecodingSucceeded = par("numTimesDecodingSucceeded");


    std::cout << " =====  SIMULATION CONFIGURATIONS ========= " << "\n";
    std::cout << " =====  numServers   : " << numServers << "         ========= \n";
    std::cout << " =====  ShortflowSize: " << flowSize << "      ========= \n";
    std::cout << " =====  numShortFlows: " << numShortFlows << "          ========= \n";
    std::cout << " =====  arrivalRate  : " << arrivalRate << "       ========= \n";
    std::cout << " ========================================== " << "\n";
    stopSimulation = new cMessage("stopSimulation");

    startManagerNode = new cMessage("startManagerNode");
    scheduleAt(0.0, startManagerNode);
}

void CentralSchedulerRaqsac::handleMessage(cMessage *msg)
{
    if (msg == stopSimulation) {
        std::cout << " All shortFlows COMPLETED  " << std::endl;
        totalSimTime = simTime();
        endSimulation();
    }
    std::cout << "******************** CentralSchedulerRaqsac::handleMessage .. ********************  \n";

    //  % of short flows and % of long flows
//    numlongflowsRunningServers = floor(numServers * 0.33); // 33% of nodes run long flows
    numlongflowsRunningServers = floor(numServers * percentLongFlowNodes); // 33% of nodes run long flows , TODO throw error as it shouldn't be 1
    numshortflowRunningServers = numServers - numlongflowsRunningServers;
    std::cout << "numshortflowRunningServers:  " << numshortflowRunningServers << std::endl;
    std::cout << "numlongflowsRunningServers:  " << numlongflowsRunningServers << std::endl;

    generateTM();
    serversLocations();

    std::string itsSrc;
    std::string newDest;

    deleteAllSubModuleApp("app[0]");

    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    std::cout << "\n\n ******************** schedule Long flows .. ********************  \n";
    scheduleLongFlows();

    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    std::cout << "\n\n ******************** schedule Short flows .. ********************  \n";
    if (oneToOne == true) {

        if (isWebSearchWorkLoad == true)
            getWebSearchWorkLoad();

        for (unsigned int i = 1; i <= numShortFlows; i++) {
            std::cout << "\n\nShortflow ID: " << i << std::endl;

            if (strcmp(trafficMatrixType, "randTM") == 0)
                getNewDestRandTM(itsSrc, newDest);
            if (strcmp(trafficMatrixType, "permTM") == 0)
                getNewDestPremTM(itsSrc, newDest);

            // identifying the servers locations: FatTree.Pod[].racks[].servers[]
            scheduleNewShortFlow(itsSrc, newDest);
        }
    }
    std::cout << "\n\n\n";

    std::cout << "\n\nCentral Scheduler complete!" << std::endl;
    std::cout << "\n\n\n";

}

void CentralSchedulerRaqsac::getNodeRackPod(unsigned int nodeIndex, unsigned int &nodeId, unsigned int &rackId, unsigned int &podId)
{
    Enter_Method
    ("getNodeRackPod(unsigned int nodeIndex,unsigned int &nodeId ,unsigned int &rackId , unsigned int &podId)");
    auto itt = nodeLocationList.begin();
    while (itt != nodeLocationList.end()) {
        if (itt->index == nodeIndex) {
            nodeId = itt->node;
            rackId = itt->rack;
            podId = itt->pod;
        }
        ++itt;
    }
}


void CentralSchedulerRaqsac::serversLocations()
{
    std::cout << "\n\n ******************** serversLocations .. ********************  \n";
    int totalNumberofPods = kValue;
    int totalNumberofRacks = (kValue / 2) * kValue;
    int racksPerPod = totalNumberofRacks / totalNumberofPods;
    int serversPerPod = pow(kValue, 2) / 4;
    int serversPerRack = kValue / 2;

    for (int m = 0; m < numServers; m++) {
        NodeLocation nodeLocation;
        nodeLocation.index = permServers.at(m);
        nodeLocation.pod = floor(permServers.at(m) / serversPerPod);
        nodeLocation.rack = floor((permServers.at(m) % serversPerPod) / serversPerRack);
        nodeLocation.node = permServers.at(m) % serversPerRack;
        nodeLocation.numRaqsacSink = 0;
        nodeLocation.numRaqsacSession = 0;
        nodeLocationList.push_back(nodeLocation);
    }

    std::list<NodeLocation>::iterator it;
    it = nodeLocationList.begin();
    while (it != nodeLocationList.end()) {
        std::cout << " index: " << it->index << " ==> " << " [pod, rack, node] =   ";
        std::cout << " [" << it->pod << ", " << it->rack << ", " << it->node << "] \n";
        it++;
    }
}

// random TM
void CentralSchedulerRaqsac::getNewDestRandTM(std::string &itsSrc, std::string &newDest)
{
    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  \n";
    std::cout << "******************** getNewDestination RandTM .. ********************  \n";
    unsigned int newDestination = 0;
    unsigned int srcNewDestination = 0;
    while (newDestination == srcNewDestination) { // the dest should be different from the src
        newDestination = permServers.at(numlongflowsRunningServers + (std::rand() % (numshortflowRunningServers)));
        srcNewDestination = permServers.at(numlongflowsRunningServers + (std::rand() % (numshortflowRunningServers)));
    }
    std::cout << "@@@ newDestination " << newDestination << " , its src   " << srcNewDestination << "\n";

    CentralSchedulerRaqsac::findLocation(newDestination, newDest);
    CentralSchedulerRaqsac::findLocation(srcNewDestination, itsSrc);

    RecordMat recordMat;
    recordMat.recordSrc = srcNewDestination;
    recordMat.recordDest = newDestination;
    recordMatList.push_back(recordMat);

    // can be replaced by recordMatList ( see Finish())
    randMapShortFlowsVec.record(srcNewDestination);
    randMapShortFlowsVec.record(newDestination);
}

// permutation TM
void CentralSchedulerRaqsac::generateTM()
{
    std::cout << "\n\n ******************** generate TM maps.. ********************  \n";
    for (int i = 0; i < numServers; ++i)
        permServers.push_back(i);
    if (shuffle)
        std::shuffle(permServers.begin(), permServers.end(), PRNG); ///////////// TODO ooooooooooooooooooo

    for (int i = 0; i < numServers; ++i) {
        if (i < numlongflowsRunningServers) {
            permLongFlowsServers.push_back(permServers.at(i));
            permMapLongFlows.insert(std::pair<int, int>(permServers.at((i + 1) % numlongflowsRunningServers), permServers.at(i)));  // < dest, src >
        }
        else if (i >= numlongflowsRunningServers && i < numServers - 1) {
            permShortFlowsServers.push_back(permServers.at(i));
            permMapShortFlows.insert(std::pair<int, int>(permServers.at(i + 1), permServers.at(i)));  // < dest, src >
        }
        else if (i == numServers - 1) {
//            permShortFlowsServers.push_back(permServers.at(numlongflowsRunningServers));
            permShortFlowsServers.push_back(permServers.at(i));
            permMapShortFlows.insert(std::pair<int, int>(permServers.at(numlongflowsRunningServers), permServers.at(i)));  // < dest, src >
        }
    }

    std::cout << "permServers:                ";
    for (std::vector<unsigned int>::iterator it = permServers.begin(); it != permServers.end(); ++it)
        std::cout << ' ' << *it;
    std::cout << '\n';

    std::cout << "permLongFlowsServers:       ";
    for (std::vector<unsigned int>::iterator it = permLongFlowsServers.begin(); it != permLongFlowsServers.end(); ++it)
        std::cout << ' ' << *it;
    std::cout << '\n';

    std::cout << "permShortFlowsServers:      ";
    for (std::vector<unsigned int>::iterator it = permShortFlowsServers.begin(); it != permShortFlowsServers.end(); ++it)
        std::cout << ' ' << *it;
    std::cout << '\n';

    std::cout << "permMapLongFlows:                 \n";
    for (std::map<unsigned int, unsigned int>::iterator iter = permMapLongFlows.begin(); iter != permMapLongFlows.end(); ++iter) {
        cout << "  src " << iter->second << " ==> ";
        cout << "  dest " << iter->first << "\n";
        permMapLongFlowsVec.record(iter->second);
        permMapLongFlowsVec.record(iter->first);
    }

    std::cout << "permMapShortFlows:                 \n";
    for (std::map<unsigned int, unsigned int>::iterator iter = permMapShortFlows.begin(); iter != permMapShortFlows.end(); ++iter) {
        cout << "   src " << iter->second << " ==> ";
        cout << "   dest " << iter->first << "\n";
        permMapShortFlowsVec.record(iter->second);
        permMapShortFlowsVec.record(iter->first);
    }

}

void CentralSchedulerRaqsac::getNewDestPremTM(std::string &itsSrc, std::string &newDest)
{
    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  \n";
    std::cout << "******************** getNewDestination PremTM .. ********************  \n";

//    int newDestination = test ;
//    test++;
//    if(test == 16) test =0;

    int newDestination = permServers.at(numlongflowsRunningServers + (std::rand() % (numshortflowRunningServers)));

    int srcNewDestination = permMapShortFlows.find(newDestination)->second;
    std::cout << "@@@ newDestination " << newDestination << " , its src   " << srcNewDestination << "\n";

    CentralSchedulerRaqsac::findLocation(newDestination, newDest);
    CentralSchedulerRaqsac::findLocation(srcNewDestination, itsSrc);

    RecordMat recordMat;
    recordMat.recordSrc = srcNewDestination;
    recordMat.recordDest = newDestination;
    recordMatList.push_back(recordMat);

    permMapShortFlowsVector.record(srcNewDestination);
    permMapShortFlowsVector.record(newDestination);
}

void CentralSchedulerRaqsac::findLocation(unsigned int nodeIndex, std::string &nodePodRackLoc)
{
    std::list<NodeLocation>::iterator itt;
    itt = nodeLocationList.begin();
    while (itt != nodeLocationList.end()) {
        if (itt->index == nodeIndex) {
            nodePodRackLoc = "FatTree.Pod[" + std::to_string(itt->pod) + "].racks[" + std::to_string(itt->rack) + "].servers[" + std::to_string(itt->node) + "]";
        }
        itt++;
    }
}

void CentralSchedulerRaqsac::scheduleLongFlows()
{
    std::cout << "\n\n ******************** scheduleLongFlows .. ********************  \n";
    std::string dest;
    std::string source;

// iterate permMapLongFlows

    for (std::map<unsigned int, unsigned int>::iterator iter = permMapLongFlows.begin(); iter != permMapLongFlows.end(); ++iter) {
        cout << "\n\n NEW LONGFLOW :)   ";
        cout << "  host(SRC.)= " << iter->second << " ==> " << "  host(DEST.)= " << iter->first << "\n";

        RecordMat recordMat;
        recordMat.recordSrc = iter->second;
        recordMat.recordDest = iter->first;
        recordMatList.push_back(recordMat);

        CentralSchedulerRaqsac::findLocation(iter->first, dest); // get dest value
        CentralSchedulerRaqsac::findLocation(iter->second, source);
        cout << "  nodePodRackLoc:  " << iter->second << " == " << source << " ==> " << iter->first << " == " << dest << "\n";

        cModule *srcModule = getModuleByPath(source.c_str());
        cModule *destModule = getModuleByPath(dest.c_str());
        // get the src RAQSAC module
        cModule *RAQSACSrcModule = srcModule->getSubmodule("at");
        cModule *RAQSACDestModule = destModule->getSubmodule("at");

        int newRAQSACGateOutSizeSrc = RAQSACSrcModule->gateSize("out") + 1;
        int newRAQSACGateInSizeSrc = RAQSACSrcModule->gateSize("in") + 1;
        int newRAQSACGateOutSizeDest = RAQSACDestModule->gateSize("out") + 1;
        int newRAQSACGateInSizeDest = RAQSACDestModule->gateSize("in") + 1;

        RAQSACSrcModule->setGateSize("out", newRAQSACGateOutSizeSrc);
        RAQSACSrcModule->setGateSize("in", newRAQSACGateInSizeSrc);
        RAQSACDestModule->setGateSize("out", newRAQSACGateOutSizeDest);
        RAQSACDestModule->setGateSize("in", newRAQSACGateInSizeDest);
        int newNumRAQSACSessionAppsSrc = findNumSumbodules(srcModule, "raqsac.application.raqsacapp.RaqsacBasicClientApp") + findNumSumbodules(srcModule, "raqsac.application.raqsacapp.RaqsacSinkApp") + 1;
        int newNumRAQSACSinkAppsDest = findNumSumbodules(destModule, "raqsac.application.raqsacapp.RaqsacSinkApp") + findNumSumbodules(destModule, "raqsac.application.raqsacapp.RaqsacBasicClientApp") + 1;
        std::cout << "Src  numRAQSACSessionApp =  " << newNumRAQSACSessionAppsSrc << "\n";
        std::cout << "Dest  NumRAQSACSinkApp   =  " << newNumRAQSACSinkAppsDest << "\n";

        // find factory object
        cModuleType *moduleTypeSrc = cModuleType::get("raqsac.application.raqsacapp.RaqsacBasicClientApp");
        cModuleType *moduleTypeDest = cModuleType::get("raqsac.application.raqsacapp.RaqsacSinkApp");

        std::string nameRAQSACAppSrc = "app[" + std::to_string(newNumRAQSACSessionAppsSrc - 1) + "]";
        std::string nameRAQSACAppDest = "app[" + std::to_string(newNumRAQSACSinkAppsDest - 1) + "]";

        // DESTttttttt  RQSinkApp
        // create (possibly compound) module and build its submodules (if any)
        cModule *newDestAppModule = moduleTypeDest->create(nameRAQSACAppDest.c_str(), destModule);
        newDestAppModule->par("localAddress").setStringValue(dest);
        newDestAppModule->par("localPort").setIntValue(80 + newNumRAQSACSessionAppsSrc);
        newDestAppModule->par("recordStatistics").setBoolValue(false);
        newDestAppModule->par("opcode").setIntValue(1);
        //   --------<RAQSACIn         appOut[]<----------
        //     RAQSACApp                          RAQSAC
        //   -------->RAQSACOut        appIn[] >----------
        cGate *gateRAQSACInDest = RAQSACDestModule->gate("in", newRAQSACGateOutSizeDest - 1);
        cGate *gateRAQSACOutDest = RAQSACDestModule->gate("out", newRAQSACGateOutSizeDest - 1);

        cGate *gateInDest = newDestAppModule->gate("socketIn");
        cGate *gateOutDest = newDestAppModule->gate("socketOut");

        gateRAQSACOutDest->connectTo(gateInDest);
        gateOutDest->connectTo(gateRAQSACInDest);

        newDestAppModule->finalizeParameters();
        newDestAppModule->buildInside();
        newDestAppModule->scheduleStart(simTime());
        newDestAppModule->callInitialize();

        // have been added few lines above ?? to remove
        //newDestAppModule->par("localPort").setIntValue(80 + newNumRAQSACSessionAppsSrc);
        //newDestAppModule->par("opcode").setIntValue(1);
        cModule *newSrcAppModule = moduleTypeSrc->create(nameRAQSACAppSrc.c_str(), srcModule);

        newSrcAppModule->par("localAddress").setStringValue(source);
        newSrcAppModule->par("connectAddress").setStringValue(dest.c_str());
        newSrcAppModule->par("connectPort").setIntValue(80 + newNumRAQSACSessionAppsSrc);
        newSrcAppModule->par("startTime").setDoubleValue(simTime().dbl());
        newSrcAppModule->par("numSymbolsToSend").setIntValue(longFlowSize);
        //newSrcAppModule->par("requestLength").setDoubleValue(10);
        //newSrcAppModule->par("replyLength").setDoubleValue(1500);
        //newSrcAppModule->par("opcode").setIntValue(2); // opcode=2 ==> Writer
        //newSrcAppModule->par("fileId").setIntValue(2017);
        //newSrcAppModule->par("isLongFlowClient").setBoolValue(true);

        //   --------<RAQSACIn         appOut[]<----------
        //     RAQSACApp                          RAQSAC
        //   -------->RAQSACOut        appIn[] >----------
        cGate *gateRAQSACIn = RAQSACSrcModule->gate("in", newRAQSACGateInSizeSrc - 1);
        cGate *gateRAQSACOut = RAQSACSrcModule->gate("out", newRAQSACGateOutSizeSrc - 1);

        cGate *gateIn = newSrcAppModule->gate("socketIn");
        cGate *gateOut = newSrcAppModule->gate("socketOut");

        gateRAQSACOut->connectTo(gateIn);
        gateOut->connectTo(gateRAQSACIn);

        newSrcAppModule->finalizeParameters();
        newSrcAppModule->buildInside();
        newSrcAppModule->scheduleStart(simTime());
        newSrcAppModule->callInitialize();
        newSrcAppModule->par("startTime").setDoubleValue(simTime().dbl());
    }
}

void CentralSchedulerRaqsac::scheduleIncast(unsigned int numSenders)
{
    std::string itsSrc;
    std::string newDest;
    unsigned int newDestination = 0;
    CentralSchedulerRaqsac::findLocation(newDestination, newDest);
    sumArrivalTimes = 0;

    // get the src RAQSAC module
    cModule *destModule = getModuleByPath(newDest.c_str());
    cModule *RAQSACDestModule = destModule->getSubmodule("at");
    unsigned int newRAQSACGateOutSizeDest = RAQSACDestModule->gateSize("out") + 1;
    unsigned int newRAQSACGateInSizeDest = RAQSACDestModule->gateSize("in") + 1;
    RAQSACDestModule->setGateSize("out", newRAQSACGateOutSizeDest);
    RAQSACDestModule->setGateSize("in", newRAQSACGateInSizeDest);
    unsigned int newNumRAQSACSinkAppsDest = findNumSumbodules(destModule, "raqsac.application.raqsacapp.RaqsacSinkApp") + 1;
    cModuleType *moduleTypeDest = cModuleType::get("raqsac.application.raqsacapp.RaqsacSinkApp");
    std::string nameRAQSACAppDest = "app[" + std::to_string(newNumRAQSACSinkAppsDest - 1) + "]";
    cModule *newDestAppModule = moduleTypeDest->create(nameRAQSACAppDest.c_str(), destModule);
    newDestAppModule->par("localPort").setIntValue(80 + newNumRAQSACSinkAppsDest);
    newDestAppModule->par("opcode").setIntValue(1);
    cGate *gateRAQSACInDest = RAQSACDestModule->gate("in", newRAQSACGateOutSizeDest - 1);
    cGate *gateRAQSACOutDest = RAQSACDestModule->gate("out", newRAQSACGateOutSizeDest - 1);
    cGate *gateInDest = newDestAppModule->gate("socketIn");
    cGate *gateOutDest = newDestAppModule->gate("socketOut");
    gateRAQSACOutDest->connectTo(gateInDest);
    gateOutDest->connectTo(gateRAQSACInDest);
    newDestAppModule->finalizeParameters();
    newDestAppModule->buildInside();
    newDestAppModule->scheduleStart(simTime());
    newDestAppModule->callInitialize();

    for (int i = 0; i < numSenders; ++i) {
        unsigned int srcNewDestination = 0;
        while (newDestination == srcNewDestination) {
            srcNewDestination = permServers.at(numlongflowsRunningServers + (std::rand() % (numshortflowRunningServers)));
        }

        CentralSchedulerRaqsac::findLocation(srcNewDestination, itsSrc);
        cModule *srcModule = getModuleByPath(itsSrc.c_str());
        cModule *RAQSACSrcModule = srcModule->getSubmodule("at");
        unsigned int newRAQSACGateOutSizeSrc = RAQSACSrcModule->gateSize("out") + 1;
        unsigned int newRAQSACGateInSizeSrc = RAQSACSrcModule->gateSize("in") + 1;
        RAQSACSrcModule->setGateSize("out", newRAQSACGateOutSizeSrc);
        RAQSACSrcModule->setGateSize("in", newRAQSACGateInSizeSrc);
        unsigned int newNumRAQSACSessionAppsSrc = findNumSumbodules(srcModule, "raqsac.application.raqsacapp.RaqsacBasicClientApp") + 1;
        cModuleType *moduleTypeSrc = cModuleType::get("raqsac.application.raqsacapp.RaqsacBasicClientApp");
        std::string nameRAQSACAppSrc = "app[" + std::to_string(newNumRAQSACSessionAppsSrc - 1) + "]";
        cModule *newSrcAppModule = moduleTypeSrc->create(nameRAQSACAppSrc.c_str(), srcModule);
        newSrcAppModule->par("connectAddress").setStringValue(newDest);
        newSrcAppModule->par("connectPort").setIntValue(80 + newNumRAQSACSinkAppsDest);  //??? to be checked
        newSrcAppModule->par("startTime").setDoubleValue(simTime().dbl() + sumArrivalTimes);
        newSrcAppModule->par("numSymbolsToSend").setIntValue(flowSize); //
        //newSrcAppModule->par("requestLength").setIntValue(10);
        newSrcAppModule->par("replyLength").setIntValue(1500);
        newSrcAppModule->par("opcode").setIntValue(2);
        newSrcAppModule->par("fileId").setIntValue(2017);
        cGate *gateRAQSACIn = RAQSACSrcModule->gate("in", newRAQSACGateInSizeSrc - 1);
        cGate *gateRAQSACOut = RAQSACSrcModule->gate("out", newRAQSACGateOutSizeSrc - 1);
        cGate *gateIn = newSrcAppModule->gate("socketIn");
        cGate *gateOut = newSrcAppModule->gate("socketOut");
        gateRAQSACOut->connectTo(gateIn);
        gateOut->connectTo(gateRAQSACIn);
        newSrcAppModule->finalizeParameters();
        newSrcAppModule->buildInside();
        newSrcAppModule->scheduleStart(simTime());
        newSrcAppModule->callInitialize();
        newSrcAppModule->par("startTime").setDoubleValue(simTime().dbl() + sumArrivalTimes);
    }
}

void CentralSchedulerRaqsac::scheduleNewShortFlow(std::string itsSrc, std::string newDest)
{
    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@ scheduleNewShortFlow .. @@@@@@@@@@@@@@@@@@@@@@@@@  \n";
    std::cout << " newDest " << newDest << "\n";
    std::cout << " itsSrc " << itsSrc << "\n";
    newArrivalTime = expDistribution.operator()(PRNG);
    sumArrivalTimes = sumArrivalTimes + newArrivalTime;
    cModule *srcModule = getModuleByPath(itsSrc.c_str());  // const char* c_str Return pointer to the string.
    cModule *destModule = getModuleByPath(newDest.c_str());

    std::cout << "srcModule:  " << srcModule->getFullPath() << "  , destModule:  " << destModule->getFullPath() << std::endl << std::endl;

    // get the src RAQSAC module
    cModule *RAQSACSrcModule = srcModule->getSubmodule("at");
    cModule *RAQSACDestModule = destModule->getSubmodule("at");

    unsigned int newRAQSACGateOutSizeSrc = RAQSACSrcModule->gateSize("out") + 1;
    unsigned int newRAQSACGateInSizeSrc = RAQSACSrcModule->gateSize("in") + 1;
    unsigned int newRAQSACGateOutSizeDest = RAQSACDestModule->gateSize("out") + 1;
    unsigned int newRAQSACGateInSizeDest = RAQSACDestModule->gateSize("in") + 1;

    RAQSACSrcModule->setGateSize("out", newRAQSACGateOutSizeSrc);
    RAQSACSrcModule->setGateSize("in", newRAQSACGateInSizeSrc);
    RAQSACDestModule->setGateSize("out", newRAQSACGateOutSizeDest);
    RAQSACDestModule->setGateSize("in", newRAQSACGateInSizeDest);
    unsigned int newNumRAQSACSessionAppsSrc = findNumSumbodules(srcModule, "raqsac.application.raqsacapp.RaqsacBasicClientApp") + findNumSumbodules(srcModule, "raqsac.application.raqsacapp.RaqsacSinkApp") + 1;
    unsigned int newNumRAQSACSinkAppsDest = findNumSumbodules(destModule, "raqsac.application.raqsacapp.RaqsacSinkApp") + findNumSumbodules(destModule, "raqsac.application.raqsacapp.RaqsacBasicClientApp") + 1;

    std::cout << "Src  numRaqsacSessionApp =  " << newNumRAQSACSessionAppsSrc << "\n";
    std::cout << "Dest  NumRaqsacSinkApp   =  " << newNumRAQSACSinkAppsDest << "\n";

    // find factory object
    cModuleType *moduleTypeSrc = cModuleType::get("raqsac.application.raqsacapp.RaqsacBasicClientApp");
    cModuleType *moduleTypeDest = cModuleType::get("raqsac.application.raqsacapp.RaqsacSinkApp");

    std::string nameRAQSACAppSrc = "app[" + std::to_string(newNumRAQSACSessionAppsSrc - 1) + "]";
    std::string nameRAQSACAppDest = "app[" + std::to_string(newNumRAQSACSinkAppsDest - 1) + "]";

    // DESTttttttt         RQSinkApp
    // create (possibly compound) module and build its submodules (if any)
    cModule *newDestAppModule = moduleTypeDest->create(nameRAQSACAppDest.c_str(), destModule);
    newDestAppModule->par("localAddress").setStringValue(newDest);
    newDestAppModule->par("localPort").setIntValue(80 + newNumRAQSACSinkAppsDest);
    newDestAppModule->par("opcode").setIntValue(1);
    //   --------<RAQSACIn         appOut[]<----------
    //     RAQSACApp                          RAQSAC
    //   -------->RAQSACOut        appIn[] >----------
    cGate *gateRAQSACInDest = RAQSACDestModule->gate("in", newRAQSACGateOutSizeDest - 1);
    cGate *gateRAQSACOutDest = RAQSACDestModule->gate("out", newRAQSACGateOutSizeDest - 1);

    cGate *gateInDest = newDestAppModule->gate("socketIn");
    cGate *gateOutDest = newDestAppModule->gate("socketOut");

    gateRAQSACOutDest->connectTo(gateInDest);
    gateOutDest->connectTo(gateRAQSACInDest);

    newDestAppModule->finalizeParameters();
    newDestAppModule->buildInside();
    newDestAppModule->scheduleStart(simTime());
    newDestAppModule->callInitialize();
    cModule *newSrcAppModule = moduleTypeSrc->create(nameRAQSACAppSrc.c_str(), srcModule);

    newSrcAppModule->par("localAddress").setStringValue(itsSrc);
    newSrcAppModule->par("connectAddress").setStringValue(newDest);
    newSrcAppModule->par("connectPort").setIntValue(80 + newNumRAQSACSinkAppsDest);  //??? to be checked
    newSrcAppModule->par("startTime").setDoubleValue(simTime().dbl() + sumArrivalTimes);

    if (isWebSearchWorkLoad == false) {
        newSrcAppModule->par("numSymbolsToSend").setIntValue(flowSize); //
        // aha .....
        unsigned int priority = getPriorityValue(flowSize);
        newSrcAppModule->par("priorityValue").setIntValue(priority); //RaptorQBasicClientApp
    }

    if (isWebSearchWorkLoad == true) {
        unsigned int newFlowSize = getNewFlowSizeFromWebSearchWorkLoad();
        newSrcAppModule->par("numSymbolsToSend").setIntValue(newFlowSize); //

        unsigned int priority = getPriorityValue(newFlowSize);
        newSrcAppModule->par("priorityValue").setIntValue(priority); //RaptorQBasicClientApp
    }

    //newSrcAppModule->par("requestLength").setIntValue(10);
    //newSrcAppModule->par("replyLength").setIntValue(1500);
    //newSrcAppModule->par("opcode").setIntValue(2);
    //newSrcAppModule->par("fileId").setIntValue(2017);

    //   --------<RAQSACIn         appOut[]<----------
    //     RAQSACApp                          RAQSAC
    //   -------->RAQSACOut        appIn[] >----------
    cGate *gateRAQSACIn = RAQSACSrcModule->gate("in", newRAQSACGateInSizeSrc - 1);
    cGate *gateRAQSACOut = RAQSACSrcModule->gate("out", newRAQSACGateOutSizeSrc - 1);

    cGate *gateIn = newSrcAppModule->gate("socketIn");
    cGate *gateOut = newSrcAppModule->gate("socketOut");

    gateRAQSACOut->connectTo(gateIn);
    gateOut->connectTo(gateRAQSACIn);

    newSrcAppModule->finalizeParameters();
    newSrcAppModule->buildInside();
    newSrcAppModule->scheduleStart(simTime());
    newSrcAppModule->callInitialize();
    newSrcAppModule->par("startTime").setDoubleValue(simTime().dbl() + sumArrivalTimes);

}

int CentralSchedulerRaqsac::findNumSumbodules(cModule *nodeModule, const char *subModuleType)
{
    int rep = 0;
    for (cModule::SubmoduleIterator iter(nodeModule); !iter.end(); iter++) {
        cModule *subModule = *iter;
        if (strcmp(subModule->getModuleType()->getFullName(), subModuleType) == 0) {
            rep++;
        }
    }
    return rep;
}

void CentralSchedulerRaqsac::deleteAllSubModuleApp(const char *subModuleToBeRemoved)
{
    std::cout << "\n\n ******************** deleteAll temp SubModuleApp  .. ********************  \n";
    std::string node;
    for (int i = 0; i < numServers; i++) {

        CentralSchedulerRaqsac::findLocation(i, node);
        cModule *nodeModule = getModuleByPath(node.c_str());

        cModule *tempRAQSACAppModule = nullptr;
        for (cModule::SubmoduleIterator iter(nodeModule); !iter.end(); iter++) {
            cModule *subModule = *iter;
            if (strcmp(subModule->getFullName(), subModuleToBeRemoved) == 0) {
                tempRAQSACAppModule = subModule;
            }
        }
        tempRAQSACAppModule->deleteModule();

        //cModule *RAQSACSrcModule = nodeModule->getSubmodule("raptorQ");
        //RAQSACSrcModule->setGateSize("appOut", 0);
        //RAQSACSrcModule->setGateSize("appIn", 0);
    }
    std::cout << " Done.. \n";

}

// permutation Traffic Matrix
void CentralSchedulerRaqsac::permTM(const char *longOrShortFlows)
{
}

double CentralSchedulerRaqsac::getNewValueFromExponentialDistribution()
{
    return expDistributionForRqDecdoing.operator()(PRNG);
}

void CentralSchedulerRaqsac::finish()
{

    for (std::vector<unsigned int>::iterator iter = permServers.begin(); iter != permServers.end(); ++iter) {
        cout << "  NODE= " << *iter << "  ";
        nodes.record(*iter);

        std::string source;
        CentralSchedulerRaqsac::findLocation(*iter, source); // get dest value
        cModule *srcModule = getModuleByPath(source.c_str());

        int finalNumRaqsacSessionApps = findNumSumbodules(srcModule, "raqsac.application.raqsacapp.RaqsacBasicClientApp");
        int finalNumRaqsacSinkApps = findNumSumbodules(srcModule, "raqsac.application.raqsacapp.RaqsacSinkApp");

        std::cout << "  finalNumRaqsacSessionApps:  " << finalNumRaqsacSessionApps << ",  finalNumRaqsacSinkApps: " << finalNumRaqsacSinkApps << "\n";
        numRaqsacSessionAppsVec.record(finalNumRaqsacSessionApps);
        numRaqsacSinkAppsVec.record(finalNumRaqsacSinkApps);
    }

    std::cout << "numshortflowRunningServers:  " << numshortflowRunningServers << std::endl;
    std::cout << "numlongflowsRunningServers:  " << numlongflowsRunningServers << std::endl;

    std::cout << "permLongFlowsServers:       ";
    for (std::vector<unsigned int>::iterator it = permLongFlowsServers.begin(); it != permLongFlowsServers.end(); ++it)
        std::cout << ' ' << *it;
    std::cout << '\n';

    // Record end time
    t2 = high_resolution_clock::now();
    auto duration = duration_cast<minutes>(t2 - t1).count();
    std::cout << "=================================================== " << std::endl;
    std::cout << " total Wall Clock Time (Real Time) = " << duration << " minutes" << std::endl;
    std::cout << " total Simulation Time      = " << totalSimTime << " sec" << std::endl;
    std::cout << "=================================================== " << std::endl;
    std::cout << "-------------------------------------------------------- " << std::endl;
    std::cout << " num completed shortflows = " << numCompletedShortFlows << std::endl;
    recordScalar("simTimeTotal=", totalSimTime);
    recordScalar("numShortFlows=", numShortFlows);
    recordScalar("flowSize=", flowSize);
    recordScalar("percentLongFlowNodes=", percentLongFlowNodes);
    recordScalar("arrivalRate=", arrivalRate);
    if (strcmp(trafficMatrixType, "permTM") == 0)
        recordScalar("permTM", 1);
    if (strcmp(trafficMatrixType, "randTM") == 0)
        recordScalar("randTM", 1);
    recordScalar("wallClockTime=", duration);
    recordScalar("IW=", IW);
    recordScalar("switchQueueLength=", switchQueueLength);

    recordScalar("perFlowEcmp=", perFlowEcmp);
    recordScalar("perPacketEcmp=", perPacketEcmp);
    recordScalar("oneToOne=", oneToOne);
    recordScalar("seedValue=", seedValue);
    recordScalar("kValue=", kValue);
    recordScalar("isWebSearchWorkLoad=", isWebSearchWorkLoad);

    recordScalar("numReplica=", numReplica);

    unsigned int numDecodingWasntNeeded = numCompletedShortFlows - numTimesDecodingSucceeded;        // >> NEW

    recordScalar("numDecodingWasntNeeded=", numDecodingWasntNeeded);        // >> NEW
    recordScalar("numDecodingFailed=", numTimesDecodingFailed); // >> NEW

    recordScalar("numTimesDecodingSucceeded=", numTimesDecodingSucceeded); // >> NEW
    std::cout << " # decoding    succeeded  =  " << numTimesDecodingSucceeded << "\n";
    std::cout << " # decoding     failed    =  " << numTimesDecodingFailed << "\n";
    std::cout << " # decoding wasn't needed =  " << numDecodingWasntNeeded << "\n"; // >> NEW
    std::cout << "-------------------------------------------------------- " << std::endl;
    std::list<RecordMat>::iterator itt;
    itt = recordMatList.begin();
    while (itt != recordMatList.end()) {
        matSrc.record(itt->recordSrc);
        matDest.record(itt->recordDest);
        itt++;
    }
}

void CentralSchedulerRaqsac::handleParameterChange(const char *parname)
{
    if (parname && strcmp(parname, "numCompletedShortFlows") == 0) {
        --numRunningShortFlowsNow;
        ++numCompletedShortFlows;
        std::cout << "======================================  " << "\n";
        std::cout << " num completed shortflows =  " << numCompletedShortFlows << "\n";
        std::cout << "======================================  " << "\n\n\n";
        if (oneToOne == true && numCompletedShortFlows == numShortFlows) {
            scheduleAt(simTime(), stopSimulation);
        }
    }

    if (parname && strcmp(parname, "numCompletedLongFlows") == 0) {
        ++numCompletedLongFlows;
    }

    if (parname && strcmp(parname, "numRunningShortFlowsNow") == 0) {
        ++numRunningShortFlowsNow;
        std::cout << " numRunningShortFlowsNow r....r..r....r....r....r...: " << numRunningShortFlowsNow << "\n\n\n";
    }

    if (parname && strcmp(parname, "numTimesDecodingSucceeded") == 0) {
        ++numTimesDecodingSucceeded;
    }

    if (parname && strcmp(parname, "numTimesDecodingFailed") == 0) {
        ++numTimesDecodingFailed;
    }

}

void CentralSchedulerRaqsac::getWebSearchWorkLoad()
{
    unsigned int numFlows = numShortFlows;
    double a[numFlows];

    std::ifstream myfile("../inputWokLoad.txt");
    if (myfile.is_open()) {
        int i = 0;
        while (i < numFlows && myfile >> a[i]) {
            flowSizeWebSeachWorkLoad.push_back(a[i]);
            i++;
        }
        myfile.close();
    }
    else
        std::cerr << "Unable to open file" << endl;
}

unsigned int CentralSchedulerRaqsac::getNewFlowSizeFromWebSearchWorkLoad()
{
    unsigned int newFLowSize = flowSizeWebSeachWorkLoad.at(indexWorkLoad);
    ++indexWorkLoad;
    return newFLowSize;
}

// auto pfabric pias homa ndp phost

// 0     --> 10KB    P=1
// 10KB  --> 100KB   P=2
// 100KB --> 1MB     P=3
// 1MB   --> 10MB    P=4
// otherwise (longflows)         P=0 (RaptorQBasicClientApp.ned --> int priorityValue = default(0);)
unsigned int CentralSchedulerRaqsac::getPriorityValue(unsigned int flowSize)
{
    unsigned int priorityValue;
    if (flowSize >= 1 && flowSize <= 7) {
        priorityValue = 1;
        return priorityValue;
    }

    if (flowSize >= 8 && flowSize <= 67) {
        priorityValue = 2;
        return priorityValue;
    }

    if (flowSize >= 68 && flowSize <= 667) {
        priorityValue = 3;
        return priorityValue;
    }

    if (flowSize >= 668 && flowSize <= 6667) {
        priorityValue = 4;
        return priorityValue;
    }

    return 0;
}

} //namespace inet

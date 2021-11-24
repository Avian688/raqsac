#include "CentralScheduler.h"

#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include <fstream>

using namespace std;
using namespace std::chrono;

namespace inet {

Define_Module(CentralScheduler);
#define PKT_SIZE 1500
CentralScheduler::~CentralScheduler()
{
    cancelAndDelete(startManagerNode);
    cancelAndDelete(stopSimulation);

}

void CentralScheduler::initialize(int stage)
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

    // multicast variables
    numReplica = par("numReplica");
    numRunningMulticastGroups = par("numRunningMulticastGroups");
    runMulticast = par("runMulticast");

    // multiSourcing
    runMultiSourcing = par("runMultiSourcing");
    numRunningMultiSourcingGroups = par("numRunningMultiSourcingGroups");
    numIncastSenders = par("numIncastSenders");

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

void CentralScheduler::handleMessage(cMessage *msg)
{
    if (msg == stopSimulation) {
        std::cout << " All shortFlows COMPLETED  " << std::endl;
        totalSimTime = simTime();
        endSimulation();
    }
    std::cout << "******************** CentralScheduler::handleMessage .. ********************  \n";

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
    if (runMulticast == false && runMultiSourcing == false && oneToOne == true) {

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

    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    if (runMulticast == true && runMultiSourcing == false && oneToOne == false) {
        std::cout << "\n\n ******************** schedule Multicast flows .. ********************  \n";
        std::cout << "******************** generate all Multicast possible Groups .. ********************  \n";
        generateMulticastGroups();   // traffic load Type I. Generate combinations list

        std::cout << "\n\n ******************** numRunningMulticastGroups= " << numRunningMulticastGroups << "  ********************  \n";
        std::cout << " ******************** & assign sender for each selected group .. ********************  \n";

        if (numRunningMulticastGroups != 0) {
            for (unsigned int i = 1; i <= numRunningMulticastGroups; i++) {
                std::cout << "numRunningMulticastGroups: " << numRunningMulticastGroups << " , and  this is the " << i << " running group" << std::endl;
                getNewMulticastCombination(); // get one group from the combinations list
            }
        }

        std::cout << "\n\n **************   MulticastGroupConn  ..    *************  \n";
        int j = 0;
        int counterI = 0;
        auto itCom = selectedCombinations.begin();
        while (itCom != selectedCombinations.end()) {
            std::cout << "[" << ++j << "] group index: " << itCom->groupIndex << " ,,  ";
            std::cout << "multicastSender: " << itCom->multicastSender << " ==> " << " Receivers GROUP =  [ ";

            for (auto iterSet = itCom->multicastReceivers.begin(); iterSet != itCom->multicastReceivers.end(); ++iterSet) {
                std::cout << *iterSet << "  ";
            }
            std::cout << " ] \n";

            std::cout << " ******************* updating MulticastGroup info (string)..'multicastGrTxRxList' ********************  \n";
            multicastGroupInfo(itCom->groupIndex, itCom->multicastSender, itCom->multicastReceivers);

            std::cout << " ******************* schedule MulticastGroupConn  .. ********************  \n";
            std::vector<int> receiversPortNumbers;

            scheduleMulticastGroupConn(itCom->groupIndex, itCom->multicastSender, itCom->multicastReceivers, receiversPortNumbers);
            std::cout << " ******************* explore the routing map in FatTree for this multicast group  .. ********************  \n";
            multicastRoutersLocation(itCom->groupIndex, itCom->multicastSender, itCom->multicastReceivers);

            std::cout << " counterI = " << ++counterI << "  \n\n\n";
            ++itCom;
        }
    }

    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$$  multiSourcing $$$$$$$$$$$$$$$$$$$$$$$$$  multiSourcing  $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    // $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
    if (runMulticast == false && runMultiSourcing == true && oneToOne == false) {
        std::cout << "\n\n ******************** schedule multiSourcing flows .. ********************  \n";
        std::cout << "******************** generate all multiSourcing possible Groups .. ********************  \n";
        generateMultiSourcingGroups(); // traffic load Type I. Generate combinations list (similar to generateMulticastGroups but we used numReplica)

        std::cout << "\n\n ******************** numRunningMultiSourcingGroups= " << numRunningMultiSourcingGroups << "  ********************  \n";
        std::cout << " ******************** & assign sender for each selected group .. ********************  \n";

        if (numRunningMultiSourcingGroups != 0) {
            for (unsigned int i = 1; i <= numRunningMultiSourcingGroups; i++) {
                std::cout << "numRunningMultiSourcingGroups: " << numRunningMultiSourcingGroups << " , and  this is the " << i << " running group" << std::endl;
                getNewMultiSourcingCombination(); // get one group from the combinations list
            }
        }
        std::cout << "\n\n **************  multiSourcing Group Conn  ..    *************  \n";
        int j = 0;
        auto itCom = multiSourcingSelectedCombinations.begin();
        while (itCom != multiSourcingSelectedCombinations.end()) {
            std::cout << "[" << ++j << "] group index: " << itCom->groupIndex << " ,,  ";
            std::cout << "multicastReceiver: " << itCom->multiSourcingReceiver << " ==> " << " Senders GROUP =  [ ";
            for (auto iterSet = itCom->multiSourcingSenders.begin(); iterSet != itCom->multiSourcingSenders.end(); ++iterSet) {
                std::cout << " iterSet multisource ... " << *iterSet << "  ";
            }
            std::cout << " ] \n";
            std::cout << "\n\n ******************* updating multiSourcingGroup info (string)..'multiSourcingGrTxRxList' ********************  \n";
            multiSourcingGroupInfo(itCom->groupIndex, itCom->multiSourcingReceiver, itCom->multiSourcingSenders);
            std::cout << "\n\n ******************* schedule  MultiSourcingGroupConn  .. ********************  \n";
            std::vector<int> sendersPortNumbers;
            scheduleMultiSourcingGroupConn(itCom->groupIndex, itCom->multiSourcingReceiver, itCom->multiSourcingSenders, sendersPortNumbers);

            //            std::cout << "\n\n ******************* explore the routing map in FatTree for this multicast group  .. ********************  \n";
            //            multicastRoutersLocation(itCom->groupIndex, itCom->multicastSender,itCom->multicastReceivers);
            std::cout << "\n\n\n";
            ++itCom;
        }

    } // runMultiSourcing == true
    std::cout << "\n\nCentral Scheduler complete!" << std::endl;
    std::cout << "\n\n\n";

}

///////  multi sourcing  111111
void CentralScheduler::generateMultiSourcingGroups()
{
    int n = numServers, k = numReplica;
    nChooseK(0, n, k, false);
    numAllCombinations = combinations.size();
    cout << "numAllCombinations =  " << numAllCombinations << " \n\n";
    // combinations LIST:
    // vector 1 (index, receiver 1, receiver 2,..), vector 2 (index, receiver 1, receiver 2,..) , ..
    list<vector<int> >::iterator iterList;
    vector<int>::iterator iterVector;
    int combinationIndex = 0;
    cout << "id:" << " Rx \n";

    for (auto iterList = combinations.begin(); iterList != combinations.end(); ++iterList) {
        // add an index (at the beginning-NB no pushfront for vector so use insert)  for each combination vector in the list
        iterList->insert(iterList->begin(), combinationIndex);
        ++combinationIndex;
        for (iterVector = iterList->begin(); iterVector != iterList->end(); ++iterVector) {
            cout << *iterVector << "  ";
        }
        cout << " \n";
    }
    cout << "================ \n\n\n";
}

///////  multi sourcing  2222222
void CentralScheduler::getNewMultiSourcingCombination()
{
    unsigned int selectedGroupIndex;
    if (randomGroup == true)
        selectedGroupIndex = std::rand() % numAllCombinations;
    if (randomGroup == false)
        selectedGroupIndex = 0;

    unsigned int selectedReceiver;
    if (randomGroup == true)
        selectedReceiver = std::rand() % numServers;
    if (randomGroup == false)
        selectedReceiver = 0;

    // we need to make sure that each group will be selected once (Traffic Load Type I).
    bool isGroupAlreadyTaken = true;
    while (isGroupAlreadyTaken == true) {
        auto it = alreadySelectedGroups.find(selectedGroupIndex);
        if (it != alreadySelectedGroups.end()) { // the selected group has already taken, its in alreadySelectedGroups==> select  another sender
            cout << "This group has been selected, so let's get another group..... " << selectedGroupIndex << " \n";
            selectedGroupIndex = std::rand() % numAllCombinations;
        }
        else {    // the selected group is new ==> DONE
            isGroupAlreadyTaken = false;
            alreadySelectedGroups.insert(selectedGroupIndex);
        }
    }

    cout << "selectedGroupIndex " << selectedGroupIndex << "  \n";
    cout << "initial selectedReceiver: " << selectedReceiver << "  \n";
    std::set<int> selectedGroup;
    auto iterList = combinations.begin();
    std::advance(iterList, selectedGroupIndex);
    int i = 0;
    cout << "multiSourcing senders group:  ";
    for (auto iterVector = iterList->begin(); iterVector != iterList->end(); ++iterVector) {
        if (i > 0) { // just to ignore the index part
            cout << *iterVector << " ";
            selectedGroup.insert(*iterVector);
        }
        ++i;
    }

    cout << " \n";
    // finding a sender which is not in the selected group, as we don't want a node to send replica to itself in this traffic load
    bool isReceiverInTheSelectedGroup = true;
    while (isReceiverInTheSelectedGroup == true) {
        auto it = selectedGroup.find(selectedReceiver);
        if (it != selectedGroup.end()) { // the selected receiver is in the selected group ==> select  another sender
            cout << "this sender node is in the selected group(it is a receiver), so let's get another sender for this group.." << *it << " \n";
            selectedReceiver = std::rand() % numServers;
        }
        else {    // the selected sender is not in the selected group ==> DONE
            isReceiverInTheSelectedGroup = false;
        }
    }

    cout << " \n";
    cout << "selectedReceiver:  " << selectedReceiver << " \n";

    cout << "multiSourcing senders group:  ";
    for (auto iter = selectedGroup.begin(); iter != selectedGroup.end(); ++iter) {
        cout << *iter << " ";
    }
    cout << "\n";

    multiSourcingGroup multiG;
    multiG.groupIndex = selectedGroupIndex;
    multiG.multiSourcingReceiver = selectedReceiver;
    multiG.multiSourcingSenders = selectedGroup;
    multiSourcingSelectedCombinations.push_back(multiG);
    cout << "--------------- \n\n\n";

}

void CentralScheduler::multiSourcingGroupInfo(unsigned int multicastGrIndex, unsigned int receiverId, std::set<int> sendersId)
{
    multiSourcingGrTxRx mTxRx;
    mTxRx.groupIndex = multicastGrIndex;
    std::string receiverString;
    findLocation(receiverId, receiverString);
    mTxRx.multiSourcingReceiver = receiverString;
    std::string rx;
    std::vector<std::string> thisGroupSenders;
    for (auto iter = sendersId.begin(); iter != sendersId.end(); ++iter) {
        findLocation(*iter, rx);
        thisGroupSenders.push_back(rx);
    }
    mTxRx.multiSourcingSenders = thisGroupSenders;
    multiSourcingGrTxRxList.push_back(mTxRx);

    // print out
    for (auto iter = multiSourcingGrTxRxList.begin(); iter != multiSourcingGrTxRxList.end(); ++iter) {
        std::cout << " Gr ID= " << iter->groupIndex << " \n";
        std::cout << " Gr Receiver= " << iter->multiSourcingReceiver << " \n";
        std::vector<std::string> senders = iter->multiSourcingSenders;
        for (auto it = senders.begin(); it != senders.end(); ++it) {
            std::cout << " Gr Senders= " << *it << " \n";
        }
    }
}

void CentralScheduler::getMultiSourcingGrReceiver(unsigned int multiSrcGrIndex, std::string &receiverName)
{

    for (auto iter = multiSourcingGrTxRxList.begin(); iter != multiSourcingGrTxRxList.end(); ++iter) {
        if (iter->groupIndex == multiSrcGrIndex) {
            receiverName = iter->multiSourcingReceiver;
        }
    }

}

void CentralScheduler::scheduleMultiSourcingGroupConn(unsigned int groupIndex, unsigned int receiverId, std::set<int> sendersGroup, std::vector<int> &sendersPortNumbers)
{
    std::cout << "!!!!! scheduleMultiSourcingGroupConn .. !!!!!!  \n";
    std::string receiverName;
    std::string senderName;

    newArrivalTime = expDistribution.operator()(PRNG);
    sumArrivalTimes = sumArrivalTimes + newArrivalTime;

    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // ^^^^^^^^^^^^^^^^^^     multiSourcing  Receiver  configurations  ^^^^^^^^^
    //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    CentralScheduler::findLocation(receiverId, receiverName);

    // get the src RAQSAC module
    cModule *destModule = getModuleByPath(receiverName.c_str());
    cModule *RAQSACDestModule = destModule->getSubmodule("at");
    int newRAQSACGateOutSizeDest = RAQSACDestModule->gateSize("out") + 1;
    int newRAQSACGateInSizeDest = RAQSACDestModule->gateSize("in") + 1;
    RAQSACDestModule->setGateSize("out", newRAQSACGateOutSizeDest);
    RAQSACDestModule->setGateSize("in", newRAQSACGateInSizeDest);
    int newNumRAQSACSinkAppsDest = findNumSumbodules(destModule, "raqsac.application.raqsacapp.RaqsacSinkApp") + 1;
    cModuleType *moduleTypeDest = cModuleType::get("raqsac.application.raqsacapp.RaqsacSinkApp");
    std::string nameRAQSACAppDest = "app[" + std::to_string(newNumRAQSACSinkAppsDest - 1) + "]";

    cModule *newDestAppModule = moduleTypeDest->create(nameRAQSACAppDest.c_str(), destModule);
    newDestAppModule->par("localPort").setIntValue(80 + newNumRAQSACSinkAppsDest); //    to check if needed
    newDestAppModule->par("multiGoupId").setIntValue(groupIndex); //    to check if needed
    newDestAppModule->par("isMultiSourcingSink").setBoolValue(true); //    to check if needed
    newDestAppModule->par("opcode").setIntValue(1);
    newDestAppModule->par("multiSrcSessionSizeBytes").setDoubleValue(PKT_SIZE * flowSize); //

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
    //        newDestAppModule->par("localPort").setDoubleValue(80 + newNumRAQSACSessionAppsSrc);
    //        newDestAppModule->par("opcode").setDoubleValue(1); // receiver
    //

    //            multicastGroupPortNumbers mlticastGrPorts;
    //            mlticastGrPorts.multicastReceiversPortNumbers = receiversPortNumbers ;
    //            mlticastGrPorts.groupIndex = groupIndex;
    //            multicastReceiversPortNumbersList.push_back(mlticastGrPorts);

    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // ^^^^^^^^^^^^^^^^^^     multiSourcing  Senders configurations  ^^^^^^^^^
    //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

    for (auto iter = sendersGroup.begin(); iter != sendersGroup.end(); ++iter) {
        cout << "NEW SENDER :)   ";
        cout << " Tx= " << *iter << "\n";
        CentralScheduler::findLocation(*iter, senderName);

        cModule *srcModule = getModuleByPath(senderName.c_str());
        cModule *RAQSACSrcModule = srcModule->getSubmodule("at");
        int newRAQSACGateOutSizeSrc = RAQSACSrcModule->gateSize("out") + 1;
        int newRAQSACGateInSizeSrc = RAQSACSrcModule->gateSize("in") + 1;
        RAQSACSrcModule->setGateSize("out", newRAQSACGateOutSizeSrc);
        RAQSACSrcModule->setGateSize("in", newRAQSACGateInSizeSrc);
        int newNumRAQSACSessionAppsSrc = findNumSumbodules(srcModule, "raqsac.application.raqsacapp.RaqsacBasicClientApp") + 1;
        cModuleType *moduleTypeSrc = cModuleType::get("raqsac.application.raqsacapp.RaqsacBasicClientApp");
        std::string nameRAQSACAppSrc = "app[" + std::to_string(newNumRAQSACSessionAppsSrc - 1) + "]";

        // multiSourcing SENDER  RaptorQBasicClientApp
        cModule *newSrcAppModule = moduleTypeSrc->create(nameRAQSACAppSrc.c_str(), srcModule);

        newSrcAppModule->par("localAddress").setStringValue(senderName.c_str()); // mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
        newSrcAppModule->par("connectAddress").setStringValue(receiverName.c_str()); // initEdgeDest initEdgeDest initEdgeDest initEdgeDest

        newSrcAppModule->par("connectPort").setIntValue(80 + newNumRAQSACSinkAppsDest); // this is not correct , use groupIndex

        sendersPortNumbers.push_back(80 + newNumRAQSACSessionAppsSrc); // ?? we might not need it

        newSrcAppModule->par("startTime").setDoubleValue(sumArrivalTimes + simTime().dbl());

        /// >>>>>>>>>>>>
        if (isWebSearchWorkLoad == false) {
//               flowSize = flowSize/3; ??????????????????
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
        /// >>>>>>>>>>>>
//           newSrcAppModule->par("numSymbolsToSend").setDoubleValue(flowSize);

        //newSrcAppModule->par("requestLength").setDoubleValue(10);
        newSrcAppModule->par("replyLength").setDoubleValue(1500);
        newSrcAppModule->par("opcode").setIntValue(2); // opcode=2 ==> Writer (sender)
        newSrcAppModule->par("fileId").setIntValue(2017);

        // NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN multiSourcing Parameters
        newSrcAppModule->par("multicastGroupIndex").setIntValue(groupIndex);
        newSrcAppModule->par("isMultiSourcingConn").setBoolValue(true); // ToDOOOOOOOOOOOO

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

bool CentralScheduler::isMulticastReceiver(unsigned int multicastGrIndex, std::string rx)
{

    for (auto iter = multicastGrTxRxList.begin(); iter != multicastGrTxRxList.end(); ++iter) {
        if (iter->groupIndex == multicastGrIndex) {
            std::vector<std::string> dests = iter->multicastReceivers;
            for (auto it = dests.begin(); it != dests.end(); ++it) {
                std::string recv = *it;
                if (strcmp(recv.c_str(), rx.c_str()) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool CentralScheduler::isMulticastSender(unsigned int multicastGrIndex, std::string tx)
{

    for (auto iter = multicastGrTxRxList.begin(); iter != multicastGrTxRxList.end(); ++iter) {
        std::string senderName = iter->multicastSender;
        if (iter->groupIndex == multicastGrIndex && strcmp(senderName.c_str(), tx.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

void CentralScheduler::getMulticastGrSender(unsigned int multicastGrIndex, std::string &senderName)
{

    for (auto iter = multicastGrTxRxList.begin(); iter != multicastGrTxRxList.end(); ++iter) {
        if (iter->groupIndex == multicastGrIndex) {
            senderName = iter->multicastSender;
        }
    }

}

void CentralScheduler::getMulticastGrFirstEdgeDest(unsigned int multicastGrIndex, std::string senderNode, std::string &firstEdge)
{

    for (auto iter = multicastInfoList.begin(); iter != multicastInfoList.end(); ++iter) {
        std::string routeName = iter->routeName;
        if (iter->multicastGroupIndex == multicastGrIndex && strcmp(senderNode.c_str(), routeName.c_str()) == 0) {
            firstEdge = iter->destinations.at(0);
        }
    }

}

void CentralScheduler::multicastGroupInfo(unsigned int multicastGrIndex, unsigned int senderId, std::set<int> receiversId)
{
    multicastGrTxRx mTxRx;
    mTxRx.groupIndex = multicastGrIndex;
    std::string senderString;
    findLocation(senderId, senderString);
    mTxRx.multicastSender = senderString;
    std::string rx;
    std::vector<std::string> thisGroupReceivers;
    for (auto iter = receiversId.begin(); iter != receiversId.end(); ++iter) {
        findLocation(*iter, rx);
        thisGroupReceivers.push_back(rx);
    }
    mTxRx.multicastReceivers = thisGroupReceivers;
    multicastGrTxRxList.push_back(mTxRx);

    // print out
    for (auto iter = multicastGrTxRxList.begin(); iter != multicastGrTxRxList.end(); ++iter) {
        std::vector<std::string> dests = iter->multicastReceivers;
        for (auto it = dests.begin(); it != dests.end(); ++it) {
        }
    }
}

void CentralScheduler::generateMulticastGroups()
{
    int n = numServers, k = numReplica;
    nChooseK(0, n, k, false);
    numAllCombinations = combinations.size();
    list<vector<int> >::iterator iterList;
    vector<int>::iterator iterVector;
    int combinationIndex = 0;
    for (auto iterList = combinations.begin(); iterList != combinations.end(); ++iterList) {
        iterList->insert(iterList->begin(), combinationIndex);
        ++combinationIndex;
        for (iterVector = iterList->begin(); iterVector != iterList->end(); ++iterVector) {
        }
    }
}

void CentralScheduler::nChooseK(int offset, int n, int k, bool r)
{
    if (r == false) {
        for (int i = 0; i < n; ++i) {
            tempNode.push_back(i);
        }
    }
    if (k == 0) {
        combinations.push_front(tempCombination); // push new combination vector(tempCombination) in the combination list(combinations)
        return;
    }
    for (int i = offset; i <= tempNode.size() - k; ++i) {
        tempCombination.push_back(tempNode[i]);
        nChooseK(i + 1, n, k - 1, true);
        tempCombination.pop_back();
    }
}

void CentralScheduler::getNewMulticastCombination()
{
    unsigned int selectedGroupIndex;
    if (randomGroup == true)
        selectedGroupIndex = std::rand() % numAllCombinations;
    if (randomGroup == false)
        selectedGroupIndex = 0;

    unsigned int selectedSender;
    if (randomGroup == true)
        selectedSender = std::rand() % numServers;
    if (randomGroup == false)
        selectedSender = 0;

    // we need to make sure that each group will be selected once (Traffic Load Type I).
    bool isGroupAlreadyTaken = true;
    while (isGroupAlreadyTaken == true) {
        auto it = alreadySelectedGroups.find(selectedGroupIndex);
        if (it != alreadySelectedGroups.end()) { // the selected group has already taken, its in alreadySelectedGroups==> select  another sender
//            cout<< "This group has been selected, so let's get another group..... " << selectedGroupIndex << " \n";
            selectedGroupIndex = std::rand() % numAllCombinations;
        }
        else {    // the selected group is new ==> DONE
            isGroupAlreadyTaken = false;
            alreadySelectedGroups.insert(selectedGroupIndex);
        }
    }

    std::set<int> selectedGroup;
    auto iterList = combinations.begin();
    std::advance(iterList, selectedGroupIndex);
    int i = 0;
    for (auto iterVector = iterList->begin(); iterVector != iterList->end(); ++iterVector) {
        if (i > 0) { // just to ignore the index part
            cout << *iterVector << " ";
            selectedGroup.insert(*iterVector);
        }
        ++i;
    }

    // finding a sender which is not in the selected group, as we don't want a node to send replica to itself in this traffic load
    bool isSenderInTheSelectedGroup = true;
    while (isSenderInTheSelectedGroup == true) {
        auto it = selectedGroup.find(selectedSender);
        if (it != selectedGroup.end()) { // the selected sender is in the selected group ==> select  another sender
            selectedSender = std::rand() % numServers;
        }
        else {                     // the selected sender is not in the selected group ==> DONE
            isSenderInTheSelectedGroup = false;
        }
    }

    multicastGroup multiG;
    multiG.groupIndex = selectedGroupIndex;
    multiG.multicastSender = selectedSender;
    multiG.multicastReceivers = selectedGroup;
    selectedCombinations.push_back(multiG);
}

void CentralScheduler::getNodeRackPod(unsigned int nodeIndex, unsigned int &nodeId, unsigned int &rackId, unsigned int &podId)
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

// return the set of receivers for the given  multicast group  index
void CentralScheduler::getMulticastGroupGivenIndex(unsigned int groupIndex, std::set<int> &multicastReceivers)
{
    Enter_Method
    ("getMulticastGroupGivenIndex(unsigned int groupIndex, std::set<int>& multicastReceivers )");
    auto itCom = selectedCombinations.begin();
    while (itCom != selectedCombinations.end()) {
        if (itCom->groupIndex == groupIndex) {
            multicastReceivers = itCom->multicastReceivers;
        }
        ++itCom;
    }
}

void CentralScheduler::getPodRackNodeForEachReceiverInMulticastGroup(std::set<int> multicastReceivers, std::set<std::string> &nodePodRackLoc)
{
    Enter_Method
    ("getPodRackNodeForEachReceiverInMulticastGroup(std::set<int> multicastReceivers , std::set<std::string>& nodePodRackLoc )");
    std::string receiverName;
    for (auto iter = multicastReceivers.begin(); iter != multicastReceivers.end(); ++iter) {
        findLocation(*iter, receiverName);
        nodePodRackLoc.insert(receiverName);
    }
}

void CentralScheduler::getMulticastGroupReceiversPortNumbers(unsigned int groupIndex, std::vector<int> &portNumbersVector)
{
    Enter_Method
    ("getMulticastGroupReceiversPortNumbers(unsigned int groupIndex,std::vector<int>& portNumbersVector) ");
    auto iter = multicastReceiversPortNumbersList.begin();
    while (iter != multicastReceiversPortNumbersList.end()) {
        if (iter->groupIndex == groupIndex) {
            portNumbersVector = iter->multicastReceiversPortNumbers;
        }
        ++iter;
    }

}

void CentralScheduler::multicastRoutersLocation(unsigned int groupIndex, unsigned int senderId, std::set<int> receiversGroup)
{
//      std::cout << " CentralScheduler::multicastRoutersLocation( )"   <<  "\n";

    std::string coreName = "none";
    std::string aggDest1 = "none";
    std::string srcNode = "none";
    std::string initDest = "none";

    std::string routerName1 = "none";
    std::string routerName2 = "none";
    std::string routerName3 = "none";
    std::string routerName4 = "none";
    std::string routerName5 = "none";

    std::string dest1 = "none";
    std::string dest2 = "none";
    std::string dest3 = "none";
    std::string dest4 = "none";
    std::string dest5 = "none";

    // multicast sender
    unsigned int senderNodeId;
    unsigned int senderRackId;
    unsigned int senderPodId;

    getNodeRackPod(senderId, senderNodeId, senderRackId, senderPodId);
    initDest = "FatTree.Pod[" + std::to_string(senderPodId) + "].racks[" + std::to_string(senderRackId) + "].edgeSwitch";
    unsigned int receiverNodeId;
    unsigned int receiverRackId;
    unsigned int receiverPodId;

    int totalNumberofCores = (kValue / 2) * (kValue / 2);
    int racksPerPod = (kValue / 2); // = number of agg in each pod

    // map each agg to its corresponding cores
    int step = 0;
    for (int i = 0; i < kValue / 2; ++i) {
        coreAggMap coreAggPairs;
        for (int j = 0; j < kValue / 2; ++j) {
            coreAggPairs.aggIndex = i;
            coreAggPairs.associatedCores.push_back(step);
            ++step;
        }
        coreAggMapList.push_back(coreAggPairs);
    }

    // lets get a random agg and its core
    unsigned int whichAggAtSenderPod = std::rand() % racksPerPod; // pick one agg router to be used for this mulsticast group
    unsigned int whichCore;
    for (auto iter = coreAggMapList.begin(); iter != coreAggMapList.end(); ++iter) {
        if (iter->aggIndex == whichAggAtSenderPod) {
            unsigned int tempCore = std::rand() % racksPerPod;
            whichCore = iter->associatedCores.at(tempCore);
        }
    }
    unsigned int whichAggAtReceiverPod = whichAggAtSenderPod;

    srcNode = "FatTree.Pod[" + std::to_string(senderPodId) + "].racks[" + std::to_string(senderRackId) + "].servers[" + std::to_string(senderNodeId) + "]";
    initDest = "FatTree.Pod[" + std::to_string(senderPodId) + "].racks[" + std::to_string(senderRackId) + "].edgeSwitch";
    aggDest1 = "FatTree.Pod[" + std::to_string(senderPodId) + "].aggRouters[" + std::to_string(whichAggAtSenderPod) + "]";
    coreName = "FatTree.CoreRouter[" + std::to_string(whichCore) + "]";
    std::vector<std::string> routingTree;

    bool samePodDiffRack = false;

    // iterate over all receivers in this group
    for (auto iter = receiversGroup.begin(); iter != receiversGroup.end(); ++iter) {
        getNodeRackPod(*iter, receiverNodeId, receiverRackId, receiverPodId);
        // CASE 1 same pod and same rack ==> edge router is used for routing
        if (receiverPodId == senderPodId && receiverRackId == senderRackId) {
            routingTree.push_back("bbbbbb");  // bbbbbb is used as a break point between the routing details for each node in this multicast group
            // sender --> edge
            routingTree.push_back(srcNode);
            routingTree.push_back(initDest);

            // edge --> receiver
            dest1 = "FatTree.Pod[" + std::to_string(receiverPodId) + "].racks[" + std::to_string(receiverRackId) + "].servers[" + std::to_string(receiverNodeId) + "]";
            routingTree.push_back(dest1);
        }

        // CASE 2 same pod but different rack
        if (receiverPodId == senderPodId && receiverRackId != senderRackId) {
            samePodDiffRack = true;
            routingTree.push_back("bbbbbb");

            // sender --> edge rack i
            routingTree.push_back(srcNode);
            routingTree.push_back(initDest);

            // edge rack i ==> agg
            dest1 = "FatTree.Pod[" + std::to_string(receiverPodId) + "].aggRouters[" + std::to_string(whichAggAtSenderPod) + "]";
            routingTree.push_back(dest1);

            // agg  ==> edge rack ii
            dest2 = "FatTree.Pod[" + std::to_string(receiverPodId) + "].racks[" + std::to_string(receiverRackId) + "].edgeSwitch";
            routingTree.push_back(dest2);

            // edge rack ii ==> receiver
            dest3 = "FatTree.Pod[" + std::to_string(receiverPodId) + "].racks[" + std::to_string(receiverRackId) + "].servers[" + std::to_string(receiverNodeId) + "]";
            routingTree.push_back(dest3);
        }

        // CASE 3 different pods
        if (receiverPodId != senderPodId) {
//            std::cout << "CASE 3: different pods. Path: sender --> edge Tx --> agg Tx --> Core --> agge Rx --> edge Rx --> receiver \n";
            routingTree.push_back("bbbbbb");

            //sender --> edge Tx
            routingTree.push_back(srcNode);
            routingTree.push_back(initDest);

            // edge Tx --> agg Tx
            dest1 = "FatTree.Pod[" + std::to_string(senderPodId) + "].aggRouters[" + std::to_string(whichAggAtSenderPod) + "]";
            routingTree.push_back(dest1);

            // agg Tx --> core
            dest2 = "FatTree.CoreRouter[" + std::to_string(whichCore) + "]";
            routingTree.push_back(dest2);

            // core --> agg Rx
            dest3 = "FatTree.Pod[" + std::to_string(receiverPodId) + "].aggRouters[" + std::to_string(whichAggAtReceiverPod) + "]";
            routingTree.push_back(dest3);

            // agg Rx --> edge Rx
            dest4 = "FatTree.Pod[" + std::to_string(receiverPodId) + "].racks[" + std::to_string(receiverRackId) + "].edgeSwitch";
            routingTree.push_back(dest4);

            // edge --> Rx
            dest5 = "FatTree.Pod[" + std::to_string(receiverPodId) + "].racks[" + std::to_string(receiverRackId) + "].servers[" + std::to_string(receiverNodeId) + "]";
            routingTree.push_back(dest5);
        }
    } // end for all Rx nodes. routingTree is read now.
    std::vector<std::string> destinationNodesForInitEdge;
    std::vector<std::string> destinationNodesForInitAgg;
    std::vector<std::string> destinationNodesForCore;

    for (auto iter = routingTree.begin(); iter != routingTree.end();) {
        if (*iter == srcNode) {        // removing the sender node from all paths
            routingTree.erase(iter);
        }
        else if (*iter == initDest) { // initDest=edge
            auto itNext = std::next(iter);
            destinationNodesForInitEdge.push_back(*itNext);
            if (*itNext != aggDest1) {
                routingTree.erase(itNext);
            }
            routingTree.erase(iter);
        }
        else if (*iter == aggDest1) {
            auto itNext = std::next(iter);
            destinationNodesForInitAgg.push_back(*itNext);
            routingTree.erase(iter);
        }
        else if (*iter == coreName) {
            auto itNext = std::next(iter);
            destinationNodesForCore.push_back(*itNext);
            routingTree.erase(iter);
        }
        else {
            ++iter;
        }
    }
    getUniqueVector(destinationNodesForInitEdge);
    getUniqueVector(destinationNodesForInitAgg);
    getUniqueVector(destinationNodesForCore);

    removeSameConsecutiveElements(routingTree);
    auto iterBack = routingTree.back(); // back() returns a reference to the last element (so don't use *iterEnd)
    if (iterBack == "bbbbbb")
        routingTree.pop_back();  // this fixed a bug of having bbbbbb as a last element in the vector

    // filling multicastInfoList with destNodes for: initEdge, initAgg, initCore
    // fill in the list that contains all nodes that are involved in multicasting and their destinations
    MulticastInfo multicastInfoSender;
    std::vector<std::string> initDestinationNodesForSender;
    initDestinationNodesForSender.push_back(initDest);
    multicastInfoSender.multicastGroupIndex = groupIndex;
    multicastInfoSender.routeName = srcNode;
    multicastInfoSender.destinations = initDestinationNodesForSender;
    multicastInfoList.push_back(multicastInfoSender);

    if (destinationNodesForInitEdge.size() != 0) {
        MulticastInfo multicastInitEdge;
        multicastInitEdge.multicastGroupIndex = groupIndex;
        multicastInitEdge.routeName = initDest;
        multicastInitEdge.destinations = destinationNodesForInitEdge;
        multicastInfoList.push_back(multicastInitEdge);
    }
    if (destinationNodesForInitAgg.size() != 0) {
        MulticastInfo multicastInitAgg;
        multicastInitAgg.multicastGroupIndex = groupIndex;
        multicastInitAgg.routeName = aggDest1;
        multicastInitAgg.destinations = destinationNodesForInitAgg;
        multicastInfoList.push_back(multicastInitAgg);
    }
    if (destinationNodesForCore.size() != 0) {
        MulticastInfo multicastCore;
        multicastCore.multicastGroupIndex = groupIndex;
        multicastCore.routeName = coreName;
        multicastCore.destinations = destinationNodesForCore;
        multicastInfoList.push_back(multicastCore);
    }

    if (routingTree.size() == 0)
        samePodDiffRack = true;
    // if the sender and all receivers are in the same rack, then routingTree.size() will be zero, so we just finished here
    if (samePodDiffRack == false) { // this fixed a bug when the sender and all receivers are in the same rack
        std::vector<std::string> leadingNodes; // the nodes after the bbbbbb marking point in routingTree
        for (auto it = routingTree.begin(); it != routingTree.end(); ++it) {
            if (*it == "bbbbbb") {
                auto itNext = std::next(it);
                leadingNodes.push_back(*itNext);
            }
        }
        getUniqueVector(leadingNodes);
        for (auto it = leadingNodes.begin(); it != leadingNodes.end(); ++it) {
            MulticastInfo multicastInfo;
            std::vector<std::string> destinationNodesForAgg;
            multicastInfo.multicastGroupIndex = groupIndex;
            multicastInfo.routeName = *it;
            for (auto iter = routingTree.begin(); iter != routingTree.end(); ++iter) {
                if (*it == *iter) {
                    auto itNext = std::next(iter);
                    destinationNodesForAgg.push_back(*itNext);
                    routingTree.erase(iter); // this might case a bug if we erase last iter, as we have ++iter in the for loop ..> TODO
                }
            }
            getUniqueVector(destinationNodesForAgg); //  this line fixed the bug where we get multiple same dest nodes for the same leadingAggNode
            multicastInfo.destinations = destinationNodesForAgg;
            multicastInfoList.push_back(multicastInfo);
        }
        // to remove any Consecutive bbbbbb marks, and to remove the last bbbbbb in routingTree(if existed)
        auto iterEnd = routingTree.back(); // back() returns a reference to the last element (so don't use *iterEnd)
        while (iterEnd == "bbbbbb") {
            routingTree.pop_back();
            iterEnd = routingTree.back();
        }
        // get rid of any extra bbbbbb
        for (auto iter = routingTree.begin(); iter != routingTree.end();) {
            auto nextIter = std::next(iter, 2);
            if (*iter == "bbbbbb" && *nextIter == "bbbbbb") {
                auto delIter = std::next(iter, 1);
                routingTree.erase(delIter);
            }
            else if (*iter == "bbbbbb" && nextIter >= routingTree.end()) {
                auto delIter = std::next(iter, 1);
                routingTree.erase(delIter);
            }
            if (nextIter < routingTree.end()) {
                ++iter;
            }
            else {
                iter = routingTree.end();
            }
        }

        // to remove any Consecutive bbbbbb marks, and to remove the last bbbbbb in routingTree(if existed)
        iterEnd = routingTree.back(); // back() returns a reference to the last element (so don't use *iterEnd)
        while (iterEnd == "bbbbbb") {
            routingTree.pop_back();
            iterEnd = routingTree.back();
        }

        removeSameConsecutiveElements(routingTree);

        ///// lasttt
        std::vector<std::string> moreLastNodes;
        for (auto it = routingTree.begin(); it != routingTree.end(); ++it) {
            if (*it == "bbbbbb") {
                auto itNext = std::next(it);
                moreLastNodes.push_back(*itNext);
            }
        }
        getUniqueVector(moreLastNodes);

        for (auto it = moreLastNodes.begin(); it != moreLastNodes.end(); ++it) {
            MulticastInfo multicastInfo;
            std::vector<std::string> destinationNodes;
            multicastInfo.multicastGroupIndex = groupIndex;
            multicastInfo.routeName = *it;
            for (auto iter = routingTree.begin(); iter != routingTree.end(); ++iter) {
                if (*it == *iter) {
                    auto itNext = std::next(iter);
                    destinationNodes.push_back(*itNext);
                    routingTree.erase(iter);
                }
            }
            multicastInfo.destinations = destinationNodes;
            multicastInfoList.push_back(multicastInfo);
        }

    }  // close this if (routingTree.size() != 0 )
    ofstream myfile;
    myfile.open("multicastingRoute.csv");
    for (auto iter = multicastInfoList.begin(); iter != multicastInfoList.end(); ++iter) {
        myfile << iter->routeName << ",";
        std::vector<std::string> destList = iter->destinations;
        for (auto it = destList.begin(); it != destList.end(); ++it) {
            myfile << *it << ",";
        }
        myfile << "bbbbbb,";
    }
    myfile.close();

//////////////////////////
    std::cout << "    \n\n\n";
}

void CentralScheduler::lookUpAtMulticastInfoList(unsigned int groupId, std::string currentNode, std::vector<std::string> &destinations)
{
    for (auto iter = multicastInfoList.begin(); iter != multicastInfoList.end(); ++iter) {
        if (iter->multicastGroupIndex == groupId && strcmp(iter->routeName.c_str(), currentNode.c_str()) == 0) {
            destinations = iter->destinations;
        }
    }

}

// remove any replicated values in the input vector, get a unique vector
void CentralScheduler::getUniqueVector(std::vector<std::string> &uniqueVector)
{
    std::vector<std::string>::iterator r, w;
    std::set<std::string> tmpset;
    for (r = uniqueVector.begin(), w = uniqueVector.begin(); r != uniqueVector.end(); ++r) {
        if (tmpset.insert(*r).second) {
            *w++ = *r;
        }
    }
    uniqueVector.erase(w, uniqueVector.end());

}

// remove any consecutive replications in the input vector
void CentralScheduler::removeSameConsecutiveElements(std::vector<std::string> &inVector)
{
    auto last = std::unique(inVector.begin(), inVector.end()); // unique forwards iterator to the new end of the range
    inVector.resize(std::distance(inVector.begin(), last));
}

void CentralScheduler::scheduleMulticastGroupConn(unsigned int groupIndex, unsigned int senderId, std::set<int> receiversGroup, std::vector<int> &receiversPortNumbers)
{
    std::string receiverName;
    std::string senderName;
    CentralScheduler::findLocation(senderId, senderName);
    unsigned int senderNodeId;
    unsigned int senderRackId;
    unsigned int senderPodId;

    getNodeRackPod(senderId, senderNodeId, senderRackId, senderPodId);
    std::string initEdgeDest = "FatTree.Pod[" + std::to_string(senderPodId) + "].racks[" + std::to_string(senderRackId) + "].edgeSwitch";

    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // ^^^^^^^^^^^^^^^^^^     Multicast  Receivers configurations  ^^^^^^^^^
    //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

    for (auto iter = receiversGroup.begin(); iter != receiversGroup.end(); ++iter) {
        cout << "NEW RECEIVER :)   ";
        CentralScheduler::findLocation(*iter, receiverName);

        // get the src RAQSAC module
        cModule *destModule = getModuleByPath(receiverName.c_str());
        cModule *RAQSACDestModule = destModule->getSubmodule("at");
        int newRAQSACGateOutSizeDest = RAQSACDestModule->gateSize("out") + 1;
        int newRAQSACGateInSizeDest = RAQSACDestModule->gateSize("in") + 1;
        RAQSACDestModule->setGateSize("out", newRAQSACGateOutSizeDest);
        RAQSACDestModule->setGateSize("in", newRAQSACGateInSizeDest);
        int newNumRAQSACSinkAppsDest = findNumSumbodules(destModule, "raqsac.application.raqsacapp.RaqsacSinkApp") + 1;
        cModuleType *moduleTypeDest = cModuleType::get("raqsac.application.raqsacapp.RaqsacSinkApp");
        std::string nameRAQSACAppDest = "app[" + std::to_string(newNumRAQSACSinkAppsDest - 1) + "]";

        // create (possibly compound) module and build its submodules (if any)
        cModule *newDestAppModule = moduleTypeDest->create(nameRAQSACAppDest.c_str(), destModule);

        newDestAppModule->par("localPort").setIntValue(80 + multicastGrpPortNum);  // groupIndex?? newNumRAQSACSinkAppsDest??
        receiversPortNumbers.push_back(80 + multicastGrpPortNum);
        newDestAppModule->par("opcode").setIntValue(1);
        newDestAppModule->par("multiSrcSessionSizeBytes").setIntValue(PKT_SIZE * flowSize); //
        newDestAppModule->par("multiGoupId").setIntValue(groupIndex); //  mmmmmmm  to check if needed
        newDestAppModule->par("isMultiSourcingSink").setBoolValue(true); //  mmmmmmm  to check if needed
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
    }

    multicastGroupPortNumbers mlticastGrPorts;
    mlticastGrPorts.multicastReceiversPortNumbers = receiversPortNumbers;
    mlticastGrPorts.groupIndex = groupIndex;
    multicastReceiversPortNumbersList.push_back(mlticastGrPorts);

    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // ^^^^^^^^^^^^^^^^^^     Multicast  Sender configurations  ^^^^^^^^^^^^
    //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // MULTICAST SENDER  NODE
    cout << "  Tx:  " << senderId << "\n";
    cModule *srcModule = getModuleByPath(senderName.c_str());
    cModule *RAQSACSrcModule = srcModule->getSubmodule("at");
    int newRAQSACGateOutSizeSrc = RAQSACSrcModule->gateSize("out") + 1;
    int newRAQSACGateInSizeSrc = RAQSACSrcModule->gateSize("in") + 1;
    RAQSACSrcModule->setGateSize("out", newRAQSACGateOutSizeSrc);
    RAQSACSrcModule->setGateSize("in", newRAQSACGateInSizeSrc);
    int newNumRAQSACSessionAppsSrc = findNumSumbodules(srcModule, "raqsac.application.raqsacapp.RaqsacBasicClientApp") + 1;
    cModuleType *moduleTypeSrc = cModuleType::get("raqsac.application.raqsacapp.RaqsacBasicClientApp");
    std::string nameRAQSACAppSrc = "app[" + std::to_string(newNumRAQSACSessionAppsSrc - 1) + "]";
    // MULTICAST SENDER  RaptorQBasicClientApp
    cModule *newSrcAppModule = moduleTypeSrc->create(nameRAQSACAppSrc.c_str(), srcModule);

    newSrcAppModule->par("localAddress").setStringValue(senderName.c_str()); // mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
    newSrcAppModule->par("connectAddress").setStringValue(initEdgeDest.c_str()); // initEdgeDest initEdgeDest initEdgeDest initEdgeDest
    newArrivalTime = expDistribution.operator()(PRNG);
    sumArrivalTimes = sumArrivalTimes + newArrivalTime;

    newSrcAppModule->par("connectPort").setIntValue(80 + multicastGrpPortNum); //newNumRAQSACSessionAppsSrc??   groupIndex??
    ++multicastGrpPortNum;
    newSrcAppModule->par("startTime").setDoubleValue(simTime().dbl() + sumArrivalTimes);

    /// >>>>>>>>>>>>
    if (isWebSearchWorkLoad == false) {
        newSrcAppModule->par("numSymbolsToSend").setIntValue(flowSize); //
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
    newSrcAppModule->par("replyLength").setIntValue(1500);
    newSrcAppModule->par("opcode").setIntValue(2); // opcode=2 ==> Writer (sender)
    newSrcAppModule->par("fileId").setIntValue(2017);

    // NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN MULTICAST Parameters
    newSrcAppModule->par("multicastGroupIndex").setIntValue(groupIndex);
    newSrcAppModule->par("isMulticastConn").setBoolValue(true);

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

}

void CentralScheduler::serversLocations()
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
void CentralScheduler::getNewDestRandTM(std::string &itsSrc, std::string &newDest)
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

    CentralScheduler::findLocation(newDestination, newDest);
    CentralScheduler::findLocation(srcNewDestination, itsSrc);

    RecordMat recordMat;
    recordMat.recordSrc = srcNewDestination;
    recordMat.recordDest = newDestination;
    recordMatList.push_back(recordMat);

    // can be replaced by recordMatList ( see Finish())
    randMapShortFlowsVec.record(srcNewDestination);
    randMapShortFlowsVec.record(newDestination);
}

// permutation TM
void CentralScheduler::generateTM()
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

void CentralScheduler::getNewDestPremTM(std::string &itsSrc, std::string &newDest)
{
    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  \n";
    std::cout << "******************** getNewDestination PremTM .. ********************  \n";

//    int newDestination = test ;
//    test++;
//    if(test == 16) test =0;

    int newDestination = permServers.at(numlongflowsRunningServers + (std::rand() % (numshortflowRunningServers)));

    int srcNewDestination = permMapShortFlows.find(newDestination)->second;
    std::cout << "@@@ newDestination " << newDestination << " , its src   " << srcNewDestination << "\n";

    CentralScheduler::findLocation(newDestination, newDest);
    CentralScheduler::findLocation(srcNewDestination, itsSrc);

    RecordMat recordMat;
    recordMat.recordSrc = srcNewDestination;
    recordMat.recordDest = newDestination;
    recordMatList.push_back(recordMat);

    permMapShortFlowsVector.record(srcNewDestination);
    permMapShortFlowsVector.record(newDestination);
}

void CentralScheduler::findLocation(unsigned int nodeIndex, std::string &nodePodRackLoc)
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

void CentralScheduler::scheduleLongFlows()
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

        CentralScheduler::findLocation(iter->first, dest); // get dest value
        CentralScheduler::findLocation(iter->second, source);
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
        newDestAppModule->par("localPort").setIntValue(80 + newNumRAQSACSessionAppsSrc);
        newDestAppModule->par("opcode").setIntValue(1);
        cModule *newSrcAppModule = moduleTypeSrc->create(nameRAQSACAppSrc.c_str(), srcModule);

        newSrcAppModule->par("connectAddress").setStringValue(dest.c_str());
        newSrcAppModule->par("connectPort").setIntValue(80 + newNumRAQSACSessionAppsSrc);
        newSrcAppModule->par("startTime").setDoubleValue(simTime().dbl());
        newSrcAppModule->par("numSymbolsToSend").setIntValue(longFlowSize); // should be longFlowSize
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

void CentralScheduler::scheduleIncast(unsigned int numSenders)
{
    std::string itsSrc;
    std::string newDest;
    unsigned int newDestination = 0;
    CentralScheduler::findLocation(newDestination, newDest);
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

        CentralScheduler::findLocation(srcNewDestination, itsSrc);
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

void CentralScheduler::scheduleNewShortFlow(std::string itsSrc, std::string newDest)
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

int CentralScheduler::findNumSumbodules(cModule *nodeModule, const char *subModuleType)
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

void CentralScheduler::deleteAllSubModuleApp(const char *subModuleToBeRemoved)
{
    std::cout << "\n\n ******************** deleteAll temp SubModuleApp  .. ********************  \n";
    std::string node;
    for (int i = 0; i < numServers; i++) {

        CentralScheduler::findLocation(i, node);
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
void CentralScheduler::permTM(const char *longOrShortFlows)
{
}

double CentralScheduler::getNewValueFromExponentialDistribution()
{
    return expDistributionForRqDecdoing.operator()(PRNG);
}

void CentralScheduler::finish()
{

    for (std::vector<unsigned int>::iterator iter = permServers.begin(); iter != permServers.end(); ++iter) {
        cout << "  NODE= " << *iter << "  ";
        nodes.record(*iter);

        std::string source;
        CentralScheduler::findLocation(*iter, source); // get dest value
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
    recordScalar("oneToMany=", runMulticast);
    recordScalar("manyToOne=", runMultiSourcing);
    recordScalar("seedValue=", seedValue);
    recordScalar("kValue=", kValue);
    recordScalar("isWebSearchWorkLoad=", isWebSearchWorkLoad);

    recordScalar("numReplica=", numReplica);
    recordScalar("numRunningMulticastGroups=", numRunningMulticastGroups);
    recordScalar("numRunningMultiSourcingGroups=", numRunningMultiSourcingGroups);

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

void CentralScheduler::handleParameterChange(const char *parname)
{
    if (parname && strcmp(parname, "numCompletedShortFlows") == 0) {
        --numRunningShortFlowsNow;
        ++numCompletedShortFlows;
        std::cout << "======================================  " << "\n";
        std::cout << " num completed shortflows =  " << numCompletedShortFlows << "\n";
        std::cout << "======================================  " << "\n\n\n";
        if (oneToOne == true && runMulticast == false && runMultiSourcing == false && numCompletedShortFlows == numShortFlows) {
            scheduleAt(simTime(), stopSimulation);
        }

        if (runMulticast == true && runMultiSourcing == false && oneToOne == false && numCompletedShortFlows == numReplica * numRunningMulticastGroups) {
            scheduleAt(simTime(), stopSimulation);
        }

        if (runMulticast == false && runMultiSourcing == true && oneToOne == false && numCompletedShortFlows == numRunningMultiSourcingGroups) {
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

void CentralScheduler::getWebSearchWorkLoad()
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

unsigned int CentralScheduler::getNewFlowSizeFromWebSearchWorkLoad()
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
unsigned int CentralScheduler::getPriorityValue(unsigned int flowSize)
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

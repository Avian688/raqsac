#ifndef __INET_CentralSchedulerRaqsac_H
#define __INET_CentralSchedulerRaqsac_H
#include <chrono>  // for high_resolution_clock

#include <map>
#include <set>
#include <algorithm>    // std::random_shuffle
#include <vector>       // std::vector
#include <random>
#include <cmath>
#include <time.h>       /* time */

#include "../../application/raqsacapp/RaqsacBasicClientApp.h"
#include "../../application/raqsacapp/RaqsacSinkApp.h"
#include "inet/common/INETDefs.h"
#include "inet/common/lifecycle/ILifecycle.h"
//#include "../rqtransportlayer/RaptorQ/RaptorQ.h"

namespace inet {
class INET_API CentralSchedulerRaqsac : public cSimpleModule, public ILifecycle
{
//private:
protected:

    bool isWebSearchWorkLoad;
    unsigned int indexWorkLoad;
    std::vector<unsigned int> flowSizeWebSeachWorkLoad;

    std::chrono::high_resolution_clock::time_point t1;
    std::chrono::high_resolution_clock::time_point t2;
    simtime_t totalSimTime;
    cOutVector permMapLongFlowsVec;
    cOutVector permMapShortFlowsVec;

    cOutVector randMapShortFlowsVec;
    cOutVector permMapShortFlowsVector;

    cOutVector numRaqsacSessionAppsVec;
    cOutVector numRaqsacSinkAppsVec;
    cOutVector nodes;
    cOutVector matSrc; // record all the source servers of the created short flows
    cOutVector matDest; // record all the dest servers of the created short flows

    cMessage *startManagerNode;
    int kValue;
    unsigned int IW;
    unsigned int switchQueueLength;
    const char *trafficMatrixType; // either "permTM"  or "randTM"
    bool perFlowEcmp;
    bool perPacketEcmp;

    unsigned int test = 0;
    unsigned int arrivalRate; // lamda of an exponential distribution (Morteza uses 256 and 2560)
    unsigned int flowSize;
    unsigned int numServers;
    unsigned int numShortFlows;
    unsigned int longFlowSize;
    double percentLongFlowNodes;
    unsigned int numCompletedShortFlows = 0;
    unsigned int numCompletedLongFlows = 0;
    unsigned int numRunningShortFlowsNow = 0;

    /////????????????????
    cMessage *stopSimulation;
    std::vector<unsigned int> permServers;

    std::vector<unsigned int> permLongFlowsServers;
    std::vector<unsigned int> permShortFlowsServers;

    unsigned int numlongflowsRunningServers; // 33% of nodes run long flows
    unsigned int numshortflowRunningServers;

    unsigned int numIncastSenders;
    ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

    // multicast variables

    unsigned int multicastGrpPortNum;

    struct coreAggMap
    {
        unsigned int aggIndex;
        std::vector<int> associatedCores; // associated

    };

    typedef std::list<coreAggMap> CoreAggMapList;
    CoreAggMapList coreAggMapList;

    bool oneToOne;
    unsigned int numReplica;
    std::vector<int> tempNode;
    std::vector<int> tempCombination;
    std::list<std::vector<int> > combinations; // all possible combinations
    unsigned int numAllCombinations;
    std::set<int> alreadySelectedGroups; // contains index of the selected groups (set.insert does not add in order like in vector.pushback)


    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override
    {
        Enter_Method_Silent
        ();
        throw cRuntimeError("Unsupported lifecycle operation '%s'", operation->getClassName());
        return true;
    }

    //  <dest, src>
    std::map<unsigned int, unsigned int> permMapLongFlows;
    std::map<unsigned int, unsigned int> permMapShortFlows;

    double sumArrivalTimes = 0;
    double newArrivalTime;
    bool shuffle = false;
    bool randomGroup = true;

    struct NodeLocation
    {
        unsigned int pod;
        unsigned int rack;
        unsigned int node;
        unsigned int index;

        unsigned int numRaqsacSink;
        unsigned int numRaqsacSession;
    };

    typedef std::list<NodeLocation> NodeLocationList;
    NodeLocationList nodeLocationList;
    unsigned int seedValue;
    std::mt19937 PRNG;
    std::exponential_distribution<double> expDistribution;
    std::exponential_distribution<double> expDistributionForRqDecdoing;

    struct RecordMat
    {
        unsigned int recordSrc;
        unsigned int recordDest;
    };
    typedef std::list<RecordMat> RecordMatList;
    RecordMatList recordMatList;

    unsigned int numTimesDecodingFailed = 0;
    unsigned int numTimesDecodingSucceeded = 0;

public:
    CentralSchedulerRaqsac()
    {
    }
    virtual ~CentralSchedulerRaqsac();
    void getNodeRackPod(unsigned int nodeIndex, unsigned int &nodeId, unsigned int &rackId, unsigned int &podId);
    double getNewValueFromExponentialDistribution();

protected:
    virtual void initialize(int stage) override;
    //virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void handleParameterChange(const char *parname) override;
    void serversLocations();
    void generateTM();

    void getNewDestRandTM(std::string &itsSrc, std::string &newDest);
    void getNewDestPremTM(std::string &itsSrc, std::string &newDest);

    void findLocation(unsigned int nodeIndex, std::string &nodePodRackLoc);
    void scheduleLongFlows();
    void deleteAllSubModuleApp(const char *subModuleToBeRemoved);
    int findNumSumbodules(cModule *nodeModule, const char *subModuleType);
    void scheduleNewShortFlow(std::string itsSrc, std::string newDest);

    void permTM(const char *longOrShortFlows);

    void scheduleIncast(unsigned int numSenders);

    void getWebSearchWorkLoad();
    unsigned int getNewFlowSizeFromWebSearchWorkLoad();
    unsigned int getPriorityValue(unsigned int flowSize);

};

}

#endif // ifndef __INET_RQ_H


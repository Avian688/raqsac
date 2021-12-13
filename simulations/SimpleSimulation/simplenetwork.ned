

package raqsac.simulations.SimpleSimulation;

@namespace(inet);
@namespace(raqsac);


import raqsac.node.StandardHostRaqsac;
import ecmp.networklayer.configurator.ipv4.Ipv4NetworkConfiguratorEcmp;
import inet.node.inet.Router;
import raqsac.base.RouterRaqsac;
import ned.DatarateChannel;
import ned.IBidirectionalChannel;

network simplenetwork
{
    parameters:
    	@display("bgb=512,395");
    	int numberOfClients = default(2);
    	int numberOfNormalFlows = default(1);
    	int numberOfLongerFlows = default(0);
        int numberOfRouters = default(1);
    types:
        channel ethernetline extends DatarateChannel
        {
            delay =  5us;
            datarate = 1Gbps;
        }
    submodules:
        client[numberOfClients]: StandardHostRaqsac {
            @display("p=44,50,m,n,$numberOfClients,150");
        }
        configurator: Ipv4NetworkConfiguratorEcmp {
            @display("p=450,350");
        }
        server: StandardHostRaqsac {
            @display("p=431,50");
        }
        router[numberOfRouters]: RouterRaqsac {
            @display("p=252,50,m,n,$numberOfRouters,150");
        }
    connections:
        for i=0..sizeof(client)-1, for j=0..sizeof(router)-1 {
            client[i].pppg++ <--> ethernetline <--> router[j].pppg++;
        }
        for i=0..sizeof(router)-1{
            router[i].pppg++ <--> ethernetline <--> server.pppg++;
        }
} 

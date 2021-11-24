package raqsac.simulations;

@namespace(inet);
@namespace(raptorq);
import raqsac.node.StandardHostRaqsac;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import ned.DatarateChannel;
import inet.common.misc.ThruputMeteringChannel;





network Incast
{
    parameters:
        int numNodes;

    types:
        channel fiberline extends ThruputMeteringChannel
        {
            per = 0;
            delay = 0s;
            datarate = 1000Mbps;
          //  thruputDisplayFormat = "u";
        }
        channel ethernetline extends ThruputMeteringChannel
        {
            per = 0;
            delay = 0s;
            datarate = 1000Mbps;
          //  thruputDisplayFormat = "u";
        }

        channel C extends DatarateChannel
        {
            datarate = 1000Mbps;
            delay = 0.1us;
        }


    submodules:
        client[numNodes]: StandardHostRaqsac;
        server: StandardHostRaqsac {
            parameters:
                @display("p=61,347;i=device/pc2");
        }
        router: Router {
            parameters:
                @display("p=162,168");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=62,119");
        }

        router1: Router {
            parameters:
                @display("p=93,213");
        }
    connections:
        for i=0..numNodes-1 {
            client[i].pppg++ <--> ethernetline <--> router.pppg++;
        }

        router1.pppg++ <--> ethernetline <--> router.pppg++;
        router1.pppg++ <--> ethernetline <--> server.pppg++;
}


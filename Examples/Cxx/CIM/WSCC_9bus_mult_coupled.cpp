/**
 * @author Markus Mirz <mmirz@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 *
 * DPsim
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <iostream>
#include <list>
#include <fstream>

#include <DPsim.h>
#include <dpsim/ThreadLevelScheduler.h>

using namespace DPsim;
using namespace CPS;

void multiply_connected(SystemTopology& sys, int copies,
	Real resistance, Real inductance, Real capacitance) {

	sys.multiply(copies);
	int counter = 0;
    std::vector<String> nodes = {"BUS5", "BUS8", "BUS6"};

	for (auto orig_node : nodes) {
		std::vector<String> nodeNames{orig_node};
    	for (int i = 2; i < copies+2; i++) {
			nodeNames.push_back(orig_node + "_" + std::to_string(i));
		}
		nodeNames.push_back(orig_node);

        // if only a single copy is added, it does not really make sense to
		// "close the ring" by adding another line
		int nlines = copies == 1 ? 1 : copies+1;
		for (int i = 0; i < nlines; i++) {
            // TODO lumped resistance?
            auto rl_node = std::make_shared<DP::SimNode>("N_add_" + std::to_string(counter));
            auto res = DP::Ph1::Resistor::make("R_" + std::to_string(counter));
            res->setParameters(resistance);
            auto ind = DP::Ph1::Inductor::make("L_" + std::to_string(counter));
            ind->setParameters(inductance);
            auto cap1 = DP::Ph1::Capacitor::make("C1_" + std::to_string(counter));
            cap1->setParameters(capacitance / 2.);
            auto cap2 = DP::Ph1::Capacitor::make("C2_" + std::to_string(counter));
            cap2->setParameters(capacitance / 2.);

            sys.addNode(rl_node);
            res->connect({sys.node<DP::SimNode>(nodeNames[i]), rl_node});
            ind->connect({rl_node, sys.node<DP::SimNode>(nodeNames[i+1])});
            cap1->connect({sys.node<DP::SimNode>(nodeNames[i]), DP::SimNode::GND});
            cap2->connect({sys.node<DP::SimNode>(nodeNames[i+1]), DP::SimNode::GND});
            counter += 1;

            sys.addComponent(res);
            sys.addComponent(ind);
            sys.addComponent(cap1);
            sys.addComponent(cap2);

			// TODO use line model
			//auto line = DP::Ph1::PiLine::make("line" + std::to_string(counter));
            //line->setParameters(resistance, inductance, capacitance);
            //line->connect({sys.node<DP::SimNode>(nodeNames[i]), sys.node<DP::SimNode>(nodeNames[i+1])});
		}
	}
}

void simulateCoupled(std::list<fs::path> filenames, Int copies, Int threads, Int seq = 0) {
	String simName = "WSCC_9bus_coupled_" + std::to_string(copies)
		+ "_" + std::to_string(threads) + "_" + std::to_string(seq);
	Logger::setLogDir("logs/"+simName);

	CIM::Reader reader(simName, Logger::Level::off, Logger::Level::off);
	SystemTopology sys = reader.loadCIM(60, filenames);

	if (copies > 0)
		multiply_connected(sys, copies, 12.5, 0.16, 1e-6);

	Simulation sim(simName, Logger::Level::off);
	sim.setSystem(sys);
	sim.setTimeStep(0.0001);
	sim.setFinalTime(0.5);
	sim.setDomain(Domain::DP);
	if (threads > 0)
		sim.setScheduler(std::make_shared<OpenMPLevelScheduler>(threads));

	// Logging
	//auto logger = DataLogger::make(simName);
	//for (Int cop = 1; cop <= copies; cop++) {
	//	for (Int bus  = 1; bus <= 9; bus++) {
	//		String attrName = "v" + std::to_string(bus) + "_" + std::to_string(cop);
	//		String nodeName = "BUS" + std::to_string(bus) + "_" + std::to_string(cop);
	//		if (cop == 1) {
	//			attrName = "v" + std::to_string(bus);
	//			nodeName = "BUS" + std::to_string(bus);
	//		}
	//		logger->addAttribute(attrName, sys.node<DP::SimNode>(nodeName)->attribute("v"));
	//	}
	//}
	//sim.addLogger(logger);

	sim.run();
	sim.logStepTimes(simName + "_step_times");
}

int main(int argc, char *argv[]) {
	CommandLineArgs args(argc, argv);

	std::list<fs::path> filenames;
	filenames = DPsim::Utils::findFiles({
		"WSCC-09_RX_DI.xml",
		"WSCC-09_RX_EQ.xml",
		"WSCC-09_RX_SV.xml",
		"WSCC-09_RX_TP.xml"
	}, "Examples/CIM/grid-data/WSCC-09/WSCC-09_RX", "CIMPATH");

	//for (Int copies = 0; copies < 20; copies++) {
	//	for (Int threads = 0; threads <= 12; threads = threads+2)
	//		simulateCoupled(filenames, copies, threads);
	//}

	std::cout << "Simulate with " << Int(args.options["copies"]) << " copies, "
		<< Int(args.options["threads"]) << " threads, sequence number "
		<< Int(args.options["seq"]) << std::endl;
	simulateCoupled(filenames, Int(args.options["copies"]),
		Int(args.options["threads"]), Int(args.options["seq"]));
}
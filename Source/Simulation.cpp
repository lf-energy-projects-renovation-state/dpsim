/** Simulation
 *
 * @author Markus Mirz <mmirz@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include "Simulation.h"
#include "CIM/Reader.h"

using namespace DPsim;

Simulation::Simulation(String name, Component::List comps, Real om, Real dt,
	Real tf, Logger::Level logLevel,
	SimulationType simType,
	Int downSampleRate) :
	mLog("Logs/" + name + ".log", logLevel),
	mLeftVectorLog("Logs/" + name + "_LeftVector.csv", logLevel),
	mRightVectorLog("Logs/" + name + "_RightVector.csv", logLevel) {

	mGnd = std::make_shared<Node>(-1);
	mName = name;
	mLogLevel = logLevel;
	mSystemModel.setSimType(simType);
	mSystemModel.setTimeStep(dt);
	mSystemModel.setOmega(om);
	mFinalTime = tf;
	mDownSampleRate = downSampleRate;

	initialize(comps);

	for (auto comp : comps) {
		mLog.Log(Logger::Level::INFO) << "Added " << comp->getType() << " '" << comp->getName() << "' to simulation." << std::endl;
	}

	mLog.Log(Logger::Level::INFO) << "System matrix:" << std::endl;
	mLog.LogMatrix(Logger::Level::INFO, mSystemModel.getCurrentSystemMatrix());
	mLog.Log(Logger::Level::INFO) << "LU decomposition:" << std::endl;
	mLog.LogMatrix(Logger::Level::INFO, mSystemModel.getLUdecomp());
	mLog.Log(Logger::Level::INFO) << "Right side vector:" << std::endl;
	mLog.LogMatrix(Logger::Level::INFO, mSystemModel.getRightSideVector());
}

Simulation::Simulation(String name,
	std::list<String> cimFiles,
	Real frequency, Real timeStep, Real finalTime,
	Logger::Level logLevel, SimulationType simType) :
	mLog("Logs/" + name + ".log", logLevel),
	mLeftVectorLog("Logs/" + name + "_LeftVector.csv", logLevel),
	mRightVectorLog("Logs/" + name + "_RightVector.csv", logLevel) {

	mGnd = std::make_shared<Node>(-1);
	mName = name;
	mLogLevel = logLevel;
	mSystemModel.setSimType(simType);
	mSystemModel.setTimeStep(timeStep);
	mSystemModel.setOmega(2*PI*frequency);
	mFinalTime = finalTime;

	CIM::Reader reader(frequency, logLevel, logLevel);

	for (String filename : cimFiles) {
		if (!reader.addFile(filename))
			std::cout << "Failed to read file " << filename << std::endl;
	}
	try {
		reader.parseFiles();
	}
	catch (...) {
		std::cerr << "Failed to parse CIM files" << std::endl;
		return;
	}
	mNodes = reader.getNodes();
	Component::List comps = reader.getComponents();
	initialize(comps);
	for (auto comp : comps) {
		mLog.Log(Logger::Level::INFO) << "Added " << comp->getType() << " '" << comp->getName() << "' to simulation." << std::endl;
	}

	mLog.Log(Logger::Level::INFO) << "System matrix:" << std::endl;
	mLog.LogMatrix(Logger::Level::INFO, mSystemModel.getCurrentSystemMatrix());
	mLog.Log(Logger::Level::INFO) << "LU decomposition:" << std::endl;
	mLog.LogMatrix(Logger::Level::INFO, mSystemModel.getLUdecomp());
	mLog.Log(Logger::Level::INFO) << "Right side vector:" << std::endl;
	mLog.LogMatrix(Logger::Level::INFO, mSystemModel.getRightSideVector());
}

void Simulation::initialize(Component::List newComponents) {
	Int maxNode = 0;
	Int currentVirtualNode = 0;

	mLog.Log(Logger::Level::INFO) << "#### Start Initialization ####" << std::endl;
	// Calculate the mNumber of nodes by going through the list of components
	// TODO we use the values from the first component vector right now and assume that
	// these values don't change on switches
	for (auto comp : newComponents) {
		// determine maximum node in component list
		if (comp->getNode1() > maxNode)
			maxNode = comp->getNode1();
		if (comp->getNode2() > maxNode)
			maxNode = comp->getNode2();
	}

	if (mNodes.size() == 0) {
		// Create Nodes for all indices
		mNodes.resize(maxNode + 1, nullptr);
		for (int index = 0; index < mNodes.size(); index++)
			mNodes[index] = std::make_shared<Node>(index);

		for (auto comp : newComponents) {
			std::shared_ptr<Node> node1, node2;
			if (comp->getNode1() < 0)
				node1 = mGnd;
			else
				node1 = mNodes[comp->getNode1()];
			if (comp->getNode2() < 0)
				node2 = mGnd;
			else
				node2 = mNodes[comp->getNode2()];

			comp->setNodes(Node::List{ node1, node2 });
		}
	}

	mLog.Log(Logger::Level::INFO) << "Maximum node number: " << maxNode << std::endl;
	// virtual nodes are placed after network nodes
	currentVirtualNode = maxNode;

	// Check if component requires virtual node and if so set one
	for (auto comp : newComponents) {
		if (comp->hasVirtualNodes()) {
			for (Int node = 0; node < comp->getVirtualNodesNum(); node++) {
				currentVirtualNode++;
				std::shared_ptr<Node> newVirtualNode = std::make_shared<Node>(currentVirtualNode);
				mNodes.push_back(newVirtualNode);
				comp->setVirtualNodeAt(newVirtualNode, node);
				mLog.Log(Logger::Level::INFO) << "Created virtual node"<< node << "=" << currentVirtualNode
					<< " for " << comp->getName() << std::endl;
			}
		}
	}

	// Calculate size of system matrix
	Int numNodes = Int(currentVirtualNode + 1);

	// Create right and left vector
	mSystemModel.initialize(numNodes);

	// Initialize right side vector and components
	mLog.Log(Logger::Level::INFO) << "Initialize power flow" << std::endl;
	for (auto comp : newComponents) {
		comp->initializePowerflow(mSystemModel.getOmega()/(2*PI));
		comp->initialize(mSystemModel);
		comp->applyRightSideVectorStamp(mSystemModel);
	}

	// Create new system matrix and apply matrix stamps
	addSystemTopology(newComponents);

	switchSystemMatrix(0);
	mComponents = mComponentsVector[0];
}

void Simulation::addSystemTopology(Component::List newComponents)
{
	mComponentsVector.push_back(newComponents);

	// It is assumed that the system size does not change
	mSystemModel.createEmptySystemMatrix();

	for (auto comp : newComponents) {
		comp->applySystemMatrixStamp(mSystemModel);
	}

	mSystemModel.addSystemMatrix();
}

Int Simulation::step(bool blocking)
{
	mSystemModel.setRightSideVectorToZero();

	for (auto eif : mExternalInterfaces) {
		eif->readValues(blocking);
	}

	for (auto comp : mComponents) {
		comp->step(mSystemModel, mTime);
	}

	mSystemModel.solve();

	for (auto comp : mComponents) {
		comp->postStep(mSystemModel);
	}

	for (auto eif : mExternalInterfaces) {
		eif->writeValues(mSystemModel);
	}

	if (mCurrentSwitchTimeIndex < mSwitchEventVector.size()) {
		if (mTime >= mSwitchEventVector[mCurrentSwitchTimeIndex].switchTime) {
			switchSystemMatrix(mSwitchEventVector[mCurrentSwitchTimeIndex].systemIndex);
			//mComponents = mComponentsVector[++mCurrentSwitchTimeIndex];

			mComponents = mComponentsVector[mSwitchEventVector[mCurrentSwitchTimeIndex].systemIndex];
			++mCurrentSwitchTimeIndex;
			mLog.Log(Logger::Level::INFO) << "Switched to system " << mCurrentSwitchTimeIndex << " at " << mTime << std::endl;
			mLog.Log(Logger::Level::INFO) << "New matrix:" << std::endl << mSystemModel.getCurrentSystemMatrix() << std::endl;
			mLog.Log(Logger::Level::INFO) << "New decomp:" << std::endl << mSystemModel.getLUdecomp() << std::endl;
		}
	}

	mLeftVectorLog.LogNodeValues(getTime(), getLeftSideVector());
	mRightVectorLog.LogNodeValues(getTime(), getRightSideVector());

	return mTime < mFinalTime;
}

void Simulation::run() {
	mLog.Log(Logger::Level::INFO) << "Start simulation." << std::endl;

	while (step()) {
		increaseByTimeStep();
	}

	mLog.Log(Logger::Level::INFO) << "Simulation finished." << std::endl;
}

void Simulation::run(double duration) {
	mLog.Log(Logger::Level::INFO) << "Run simulation for " << duration << " seconds." << std::endl;

	double started = mTime;

	while (step()) {
		increaseByTimeStep();

		if (mTime - started > duration)
			break;
	}
	mLog.Log(Logger::Level::INFO) << "Simulation finished." << std::endl;
}

void Simulation::switchSystemMatrix(Int systemMatrixIndex)
{
	mSystemModel.switchSystemMatrix(systemMatrixIndex);
}

void Simulation::setSwitchTime(Real switchTime, Int systemIndex)
{
	switchConfiguration newSwitchConf;
	newSwitchConf.switchTime = switchTime;
	newSwitchConf.systemIndex = systemIndex;
	mSwitchEventVector.push_back(newSwitchConf);
}

void Simulation::increaseByTimeStep()
{
	mTime = mTime + mSystemModel.getTimeStep();
}

void Simulation::addExternalInterface(ExternalInterface *eint)
{
	mExternalInterfaces.push_back(eint);
}

void Simulation::setNumericalMethod(NumericalMethod numMethod)
{
	mSystemModel.setNumMethod(numMethod);
}

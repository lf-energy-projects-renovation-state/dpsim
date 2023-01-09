/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#pragma once

#include <dpsim-models/SimPowerComp.h>
#include <dpsim-models/Solver/MNAInterface.h>
#include <dpsim-models/EMT/EMT_Ph1_VoltageSource.h>

namespace CPS {
namespace EMT {
namespace Ph1 {
	class VoltageSourceRamp :
		public SimPowerComp<Real>,
		public MNAInterface,
		public SharedFactory<VoltageSourceRamp> {
	protected:
		///
		Complex mAddVoltage;
		///
		Real mSwitchTime;
		///
		Real mAddSrcFreq;
		///
		Real mRampTime;
		///
		std::shared_ptr<VoltageSource> mSubVoltageSource;
	public:
		///
		const Attribute<Complex>::Ptr mVoltageRef;
		///
		const Attribute<Real>::Ptr mSrcFreq;
		/// Defines UID, name and logging level
		VoltageSourceRamp(String uid, String name, Logger::Level logLevel = Logger::Level::off);
		/// Defines name and logging level
		VoltageSourceRamp(String name, Logger::Level logLevel = Logger::Level::off)
			: VoltageSourceRamp(name, name, logLevel) { }

		SimPowerComp<Real>::Ptr clone(String name);

		// #### General ####
		/// Initializes component from power flow data
		void initializeFromNodesAndTerminals(Real frequency) { }
		///
		void setParameters(Complex voltage, Complex addVoltage,
			Real srcFreq, Real addSrcFreq, Real switchTime, Real rampTime);
		///
		void initialize(Matrix frequencies);

		// #### MNA section ####
		/// Initializes internal variables of the component
		void mnaInitialize(Real omega, Real timeStep, Attribute<Matrix>::Ptr leftVector);
		/// Stamps system matrix
		void mnaApplySystemMatrixStamp(Matrix& systemMatrix);
		/// Stamps right side (source) vector
		void mnaApplyRightSideVectorStamp(Matrix& rightVector);

		void updateState(Real time);

		class MnaPreStep : public Task {
		public:
			MnaPreStep(VoltageSourceRamp& voltageSource) :
				Task(**voltageSource.mName + ".MnaPreStep"), mVoltageSource(voltageSource) {
			}

			void execute(Real time, Int timeStepCount);

		private:
			VoltageSourceRamp& mVoltageSource;
		};
	};
}
}
}
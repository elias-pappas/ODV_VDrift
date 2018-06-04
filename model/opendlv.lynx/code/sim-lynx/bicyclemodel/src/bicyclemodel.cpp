/**
* Copyright (C) 2017 Chalmers Revere
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

#include <iostream>

#include <opendavinci/odcore/data/TimeStamp.h>
#include <opendavinci/odcore/strings/StringToolbox.h>

#include "bicyclemodel.hpp"

namespace opendlv {
namespace sim {
namespace lynx {

BicycleModel::BicycleModel(int32_t const &a_argc, char **a_argv) :
  TimeTriggeredConferenceClientModule(a_argc, a_argv, "sim-lynx-bicyclemodel"),
  m_groundAccelerationMutex{},
  m_groundSteeringAngleMutex{},
  m_groundAcceleration{0.0},
  m_groundSteeringAngle{0.0}
{
}


BicycleModel::~BicycleModel()
{
}

void BicycleModel::nextContainer(odcore::data::Container &a_container)
{
  std::cout << "nextContainer " << std::endl;
  if (a_container.getDataType() == opendlv::proxy::GroundDecelerationRequest::ID()) {
    odcore::base::Lock l(m_groundAccelerationMutex);
    auto groundDeceleration = a_container.getData<opendlv::proxy::GroundDecelerationRequest>();
    m_groundAcceleration = -groundDeceleration.getGroundDeceleration();
  } else if (a_container.getDataType() == opendlv::proxy::GroundAccelerationRequest::ID()) {
    odcore::base::Lock l(m_groundAccelerationMutex);
    auto groundAcceleration = a_container.getData<opendlv::proxy::GroundAccelerationRequest>();
    m_groundAcceleration = groundAcceleration.getGroundAcceleration();
  } else if (a_container.getDataType() == opendlv::proxy::GroundSteeringRequest::ID()) {
    odcore::base::Lock m(m_groundSteeringAngleMutex);
    auto groundSteeringAngle = a_container.getData<opendlv::proxy::GroundSteeringRequest>();
    m_groundSteeringAngle = groundSteeringAngle.getGroundSteering();
  }
}

odcore::data::dmcp::ModuleExitCodeMessage::ModuleExitCode BicycleModel::body()
{
  std::cout << "body " << std::endl;
  double const g = 9.82f;

  auto kv = getKeyValueConfiguration();
  double const mass = kv.getValue<double>("sim-lynx-bicyclemodel.mass");
  double const momentOfInertiaZ = kv.getValue<double>("sim-lynx-bicyclemodel.momentOfInertiaZ");
  double const length = kv.getValue<double>("sim-lynx-bicyclemodel.length");
  double const frontToCog = kv.getValue<double>("sim-lynx-bicyclemodel.frontToCog");
  double const rearToCog = length - frontToCog;
  double const frictionCoefficient = kv.getValue<double>("sim-lynx-bicyclemodel.frictionCoefficient");

  double const magicFormulaCAlpha = kv.getValue<double>("sim-lynx-bicyclemodel.magicFormulaCAlpha");
  double const magicFormulaC = kv.getValue<double>("sim-lynx-bicyclemodel.magicFormulaC");
  double const magicFormulaE = kv.getValue<double>("sim-lynx-bicyclemodel.magicFormulaE");

  double const dt = 1.0 / static_cast<double>(getFrequency());

  double longitudinalSpeed{0.0};
  double lateralSpeed{0.0};
  double yawRate{0.0};

  while (getModuleStateAndWaitForRemainingTimeInTimeslice() == odcore::data::dmcp::ModuleStateMessage::RUNNING) {
    std::cout << "LOOP " << std::endl;
    // TODO: Is this really what we want? The vehicle can never reverse or stop.
    if (longitudinalSpeed < 0.0001) {
      longitudinalSpeed = 0.01;
    }

    double groundAccelerationCopy;
    double groundSteeringAngleCopy;
    {
      odcore::base::Lock l(m_groundAccelerationMutex);
      odcore::base::Lock m(m_groundSteeringAngleMutex);
      groundAccelerationCopy = m_groundAcceleration;
      groundSteeringAngleCopy = m_groundSteeringAngle;
    }

    double slipAngleFront = groundSteeringAngleCopy - std::atan(
        (lateralSpeed + frontToCog * yawRate) / std::abs(longitudinalSpeed));
    double slipAngleRear = -std::atan((lateralSpeed - rearToCog * yawRate) /
        std::abs(longitudinalSpeed));

    double forceFrontZ = mass * g * (frontToCog / (frontToCog + length));
    double forceRearZ = mass * g * (length / (frontToCog + length));

    double forceFrontY = magicFormula(slipAngleFront, forceFrontZ,
        frictionCoefficient, magicFormulaCAlpha, magicFormulaC, magicFormulaE);
    double forceRearY = magicFormula(slipAngleRear, forceRearZ,
        frictionCoefficient, magicFormulaCAlpha, magicFormulaC, magicFormulaE);

    double longitudinalSpeedDot = groundAccelerationCopy -
      forceFrontY * std::sin(groundSteeringAngleCopy) / mass +
      yawRate * lateralSpeed;

    double lateralSpeedDot =
      (forceFrontY * std::cos(groundSteeringAngleCopy) + forceRearY) / mass -
      yawRate * lateralSpeed;

    double yawRateDot = (length * forceFrontY *
        std::cos(groundSteeringAngleCopy) - frontToCog * forceRearY) /
      momentOfInertiaZ;

    longitudinalSpeed += longitudinalSpeedDot * dt;
    lateralSpeed += lateralSpeedDot * dt;
    yawRate += yawRateDot * dt;

    opendlv::sim::KinematicState kinematicState;
    kinematicState.setVx(longitudinalSpeed);
    kinematicState.setVy(lateralSpeed);
    kinematicState.setYawRate(yawRate);

    odcore::data::Container c(kinematicState);
    getConference().send(c);
  }
  std::cout << "LOL " << std::endl;
  return odcore::data::dmcp::ModuleExitCodeMessage::OKAY;
}


void BicycleModel::setUp()
{
}

void BicycleModel::tearDown()
{
}

double BicycleModel::magicFormula(double const &a_slipAngle, double const &a_forceZ,
    double const &a_frictionCoefficient, double const &a_cAlpha, double const &a_c,
    double const &a_e) const
{
  double const b = a_cAlpha / (a_c * a_frictionCoefficient * a_forceZ);
  double const forceY = a_frictionCoefficient * a_forceZ * std::sin(a_c *
     std::atan(b * a_slipAngle - a_e * (b * a_slipAngle - std::atan(b * a_slipAngle))));
  return forceY;
}

}
}
}
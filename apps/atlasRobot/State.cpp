/*
 * Copyright (c) 2014, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Jeongseok Lee <jslee02@gmail.com>
 *
 * Georgia Tech Graphics Lab and Humanoid Robotics Lab
 *
 * Directed by Prof. C. Karen Liu and Prof. Mike Stilman
 * <karenliu@cc.gatech.edu> <mstilman@cc.gatech.edu>
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include "apps/atlasRobot/State.h"

#include "dart/common/Console.h"
#include "dart/math/Helpers.h"
#include "dart/dynamics/Skeleton.h"
#include "dart/dynamics/BodyNode.h"
#include "dart/dynamics/Joint.h"
#include "dart/dynamics/GenCoord.h"
#include "dart/dynamics/Shape.h"
//#include "dart/constraint/OldConstraintDynamics.h"
#include "dart/collision/CollisionDetector.h"

#include "apps/atlasRobot/TerminalCondition.h"

// Macro for functions not implemented yet
#define NOT_YET(FUNCTION) std::cout << #FUNCTION\
                                  << "Not implemented yet."\
                                  << std::endl;

using namespace std;

using namespace Eigen;

using namespace dart::dynamics;
using namespace dart::constraint;

//==============================================================================
State::State(Skeleton* _skeleton, const std::string& _name)
  : mSkeleton(_skeleton),
    mName(_name),
    mNextState(this),
    mBeginTime(0.0),
    mEndTime(0.0),
    mFrame(0),
    mElapsedTime(0.0),
    mDesiredGlobalSwingLegAngleOnSagital(0.0),
    mDesiredGlobalSwingLegAngleOnCoronal(0.0),
    mDesiredGlobalPelvisAngleOnSagital(0.0),
    mDesiredGlobalPelvisAngleOnCoronal(0.0)
{
  int dof = mSkeleton->getNumGenCoords();

  mDesiredJointPositions        = Eigen::VectorXd::Zero(dof);
  mDesiredJointPositionsBalance = Eigen::VectorXd::Zero(dof);
  mKp = Eigen::VectorXd::Zero(dof);
  mKd = Eigen::VectorXd::Zero(dof);
  mSagitalCd = Eigen::VectorXd::Zero(dof);
  mSagitalCv = Eigen::VectorXd::Zero(dof);
  mCoronalCd = Eigen::VectorXd::Zero(dof);
  mCoronalCv = Eigen::VectorXd::Zero(dof);
  mTorque = Eigen::VectorXd::Zero(dof);

  for (int i = 0; i < dof; ++i)
  {
    mKp[i] = ATLAS_DEFAULT_KP;
    mKd[i] = ATLAS_DEFAULT_KD;
  }

  _buildJointMap();

  mPelvis     = mSkeleton->getBodyNode("pelvis");
  mLeftFoot   = mSkeleton->getBodyNode("l_foot");
  mRightFoot  = mSkeleton->getBodyNode("r_foot");
  mLeftThigh  = mSkeleton->getBodyNode("l_uleg");
  mRightThigh = mSkeleton->getBodyNode("r_uleg");
  mStanceFoot = NULL;

  assert(mPelvis     != NULL);
  assert(mLeftFoot   != NULL);
  assert(mRightFoot  != NULL);
  assert(mLeftThigh  != NULL);
  assert(mRightThigh != NULL);
//  assert(mStanceFoot != NULL);
}

//==============================================================================
State::~State()
{

}

//==============================================================================
void State::setName(string& _name)
{
  mName = _name;
}

//==============================================================================
const string& State::getName() const
{
  return mName;
}

//==============================================================================
void State::setNextState(State* _nextState)
{
  mNextState = _nextState;
}

//==============================================================================
void State::setTerminalCondition(TerminalCondition* _condition)
{
  assert(_condition != NULL);

  mTerminalCondition = _condition;
}

//==============================================================================
void State::begin(double _currentTime)
{
  mBeginTime = _currentTime;
  mFrame = 0;
  mElapsedTime = 0.0;
}

//==============================================================================
void State::computeControlForce(double _timestep)
{
  assert(mNextState != NULL && "Next state should be set.");

  int dof = mSkeleton->getNumGenCoords();
  VectorXd q = mSkeleton->getConfigs();
  VectorXd dq = mSkeleton->getGenVels();

  // Compute relative joint angles from desired global angles of the pelvis and
  // the swing leg


  // Update desired joint angles with balance feedback. Equation (1) in the
  // paper
  mDesiredJointPositionsBalance = mDesiredJointPositions
      + getSagitalCOMDistance() * mSagitalCd
      + getSagitalCOMVelocity() * mSagitalCv
      + getCoronalCOMDistance() * mCoronalCd
      + getCoronalCOMVelocity() * mCoronalCv;

  //  cout << "Sagital D: " << getSagitalCOMDistance() << endl;
  //  cout << "Sagital V: " << getSagitalCOMVelocity() << endl;
  //  cout << endl;
  //  cout << "Coronal D: " << getCoronalCOMDistance() << endl;
  //  cout << "Coronal V: " << getCoronalCOMVelocity() << endl;
  //  cout << endl;

//  cout << "Sagital left thigh : " << DART_DEGREE * getSagitalLeftLegAngle() << endl;
//  cout << "Sagital right thigh: " << DART_DEGREE * getSagitalRightLegAngle() << endl;
//  cout << endl;
//  cout << "Coronal left thigh : " << DART_DEGREE * getCoronalLeftLegAngle() << endl;
//  cout << "Coronal right thigh: " << DART_DEGREE * getCoronalRightLegAngle() << endl;
//  cout << endl;

//  cout << "Sagital pelvis: " << DART_DEGREE * getSagitalPelvisAngle() << endl;
//  cout << "Coronal pelvis: " << DART_DEGREE * getCoronalPelvisAngle() << endl;
//  cout << endl;

  // Compute torques for all the joints except for hip (standing and swing)
  // joints. The first 6 dof is for base body force so it is set to zero.
  mTorque.head<6>() = Vector6d::Zero();
  for (int i = 6; i < dof; ++i)
  {
    mTorque[i] = -mKp[i] * (q[i] - mDesiredJointPositionsBalance[i])
                 -mKd[i] * dq[i];
  }
//  cout << "q: " << q.transpose() << endl;
//  cout << "dq: " << dq.transpose() << endl;
//  cout << "mKp: " << mKp.transpose() << endl;
//  cout << "mKd: " << mKd.transpose() << endl;
//  cout << "mTorque: " << mTorque.transpose() << endl;
//  cout << "Theta_d: " << mDesiredJointPositionsBalance.transpose() << endl;

  // Torso and swing-hip control
  _updateTorqueForStanceLeg();

  // Apply control torque to the skeleton
  mSkeleton->setInternalForceVector(mTorque);

  mElapsedTime += _timestep;
  mFrame++;
}

//==============================================================================
bool State::isTerminalConditionSatisfied() const
{
  assert(mTerminalCondition != NULL && "Invalid terminal condition.");

  return mTerminalCondition->isSatisfied();
}

//==============================================================================
void State::end(double _currentTime)
{
  mEndTime = _currentTime;
}

//==============================================================================
Eigen::Vector3d State::getCOM() const
{
  return mSkeleton->getWorldCOM();
}

//==============================================================================
Eigen::Vector3d State::getCOMVelocity() const
{
  return mSkeleton->getWorldCOMVelocity();
}

//==============================================================================
Eigen::Isometry3d State::getCOMFrame() const
{
  Eigen::Isometry3d T = Eigen::Isometry3d::Identity();

  // Y-axis
  Eigen::Vector3d yAxis = Eigen::Vector3d::UnitY();

  // X-axis
  Eigen::Vector3d xAxis = mPelvis->getWorldTransform().linear().col(0);
  Eigen::Vector3d pelvisXAxis = mPelvis->getWorldTransform().linear().col(0);
  double mag = yAxis.dot(pelvisXAxis);
  pelvisXAxis -= mag * yAxis;
  xAxis = pelvisXAxis.normalized();

  // Z-axis
  Eigen::Vector3d zAxis = xAxis.cross(yAxis);

  T.translation() = getCOM();

  T.linear().col(0) = xAxis;
  T.linear().col(1) = yAxis;
  T.linear().col(2) = zAxis;

  return T;
}

//==============================================================================
double State::getSagitalCOMDistance()
{
  Eigen::Vector3d xAxis = getCOMFrame().linear().col(0);  // x-axis
  Eigen::Vector3d d = getCOM() - getStanceAnklePosition();

  return d.dot(xAxis);
}

//==============================================================================
double State::getSagitalCOMVelocity()
{
  Eigen::Vector3d xAxis = getCOMFrame().linear().col(0);  // x-axis
  Eigen::Vector3d v = getCOMVelocity();

  return v.dot(xAxis);
}

//==============================================================================
double State::getCoronalCOMDistance()
{
  Eigen::Vector3d yAxis = getCOMFrame().linear().col(2);  // z-axis
  Eigen::Vector3d d = getCOM() - getStanceAnklePosition();

  return d.dot(yAxis);
}

//==============================================================================
double State::getCoronalCOMVelocity()
{
  Eigen::Vector3d yAxis = getCOMFrame().linear().col(2);  // z-axis
  Eigen::Vector3d v = getCOMVelocity();

  return v.dot(yAxis);
}

//==============================================================================
Eigen::Vector3d State::getStanceAnklePosition() const
{
  if (mStanceFoot == NULL)
    return getCOM();
  else
    return _getJointPosition(mStanceFoot);
}

//==============================================================================
Eigen::Vector3d State::getLeftAnklePosition() const
{
  return _getJointPosition(mLeftFoot);
}

//==============================================================================
Eigen::Vector3d State::getRightAnklePosition() const
{
  return _getJointPosition(mRightFoot);
}

//==============================================================================
double State::getSagitalPelvisAngle() const
{
  Matrix3d comR = getCOMFrame().linear();
  Vector3d comY = comR.col(1);

  Vector3d pelvisZ = mPelvis->getWorldTransform().linear().col(2);
  Vector3d projPelvisZ = (comR.transpose() * pelvisZ);
  projPelvisZ[2] = 0.0;
  projPelvisZ.normalize();
  double angle = _getAngleBetweenTwoVectors(projPelvisZ, comY);

  Vector3d cross = comY.cross(projPelvisZ);

  if (cross[2] > 0.0)
    return angle;
  else
    return -angle;
}

//==============================================================================
double State::getCoronalPelvisAngle() const
{
  Matrix3d comR = getCOMFrame().linear();
  Vector3d comY = comR.col(1);
  Vector3d pelvisZ = mPelvis->getWorldTransform().linear().col(2);
  Vector3d projPelvisZ = (comR.transpose() * pelvisZ);
  projPelvisZ[0] = 0.0;
  projPelvisZ.normalize();
  double angle = _getAngleBetweenTwoVectors(projPelvisZ, comY);

  Vector3d cross = comY.cross(projPelvisZ);

  if (cross[0] > 0.0)
    return angle;
  else
    return -angle;
}

//==============================================================================
double State::getSagitalLeftLegAngle() const
{
  Matrix3d comR = getCOMFrame().linear();
  Vector3d comY = comR.col(1);
  Vector3d thighAxisZ = mLeftThigh->getWorldTransform().linear().col(2);
  Vector3d projThighAZ = (comR.transpose() * thighAxisZ);
  projThighAZ[2] = 0.0;
  projThighAZ.normalize();
  double angle = _getAngleBetweenTwoVectors(projThighAZ, comY);

  Vector3d cross = comY.cross(projThighAZ);

  if (cross[2] > 0.0)
    return angle;
  else
    return -angle;
}

//==============================================================================
double State::getSagitalRightLegAngle() const
{
  Matrix3d comR = getCOMFrame().linear();
  Vector3d comY = comR.col(1);
  Vector3d thighAxisZ = mRightThigh->getWorldTransform().linear().col(2);
  Vector3d projThighAZ = (comR.transpose() * thighAxisZ);
  projThighAZ[2] = 0.0;
  projThighAZ.normalize();
  double angle = _getAngleBetweenTwoVectors(projThighAZ, comY);

  Vector3d cross = comY.cross(projThighAZ);

  if (cross[2] > 0.0)
    return angle;
  else
    return -angle;
}

//==============================================================================
double State::getCoronalLeftLegAngle() const
{
  Matrix3d comR = getCOMFrame().linear();
  Vector3d comY = comR.col(1);
  Vector3d thighAxisZ = mLeftThigh->getWorldTransform().linear().col(2);
  Vector3d projThighAZ = (comR.transpose() * thighAxisZ);
  projThighAZ[0] = 0.0;
  projThighAZ.normalize();
  double angle = _getAngleBetweenTwoVectors(projThighAZ, comY);

  Vector3d cross = comY.cross(projThighAZ);

  if (cross[0] > 0.0)
    return angle;
  else
    return -angle;
}

//==============================================================================
double State::getCoronalRightLegAngle() const
{
  Matrix3d comR = getCOMFrame().linear();
  Vector3d comY = comR.col(1);
  Vector3d thighAxisZ = mRightThigh->getWorldTransform().linear().col(2);
  Vector3d projThighAZ = (comR.transpose() * thighAxisZ);
  projThighAZ[0] = 0.0;
  projThighAZ.normalize();
  double angle = _getAngleBetweenTwoVectors(projThighAZ, comY);

  Vector3d cross = comY.cross(projThighAZ);

  if (cross[0] > 0.0)
    return angle;
  else
    return -angle;
}

//==============================================================================
void State::_buildJointMap()
{
  mJointMap.clear();

  for (int i = 0; i < mSkeleton->getNumBodyNodes(); ++i)
    mJointMap[mSkeleton->getJoint(i)->getName()] = i;
}

//==============================================================================
Eigen::Vector3d State::_getJointPosition(BodyNode* _bodyNode) const
{
  Joint* parentJoint = _bodyNode->getParentJoint();
  Eigen::Vector3d localJointPosition
      = parentJoint->getTransformFromChildBodyNode().translation();
  return _bodyNode->getWorldTransform() * localJointPosition;
}


//==============================================================================
double State::_getAngleBetweenTwoVectors(const Eigen::Vector3d& _v1,
                                         const Eigen::Vector3d& _v2) const
{
  return std::acos(_v1.dot(_v2) / (_v1.norm() * _v2.norm()));
}

//==============================================================================
void State::_updateTorqueForStanceLeg()
{
  // Stance leg is left leg
  if (mStanceFoot == mLeftFoot)
  {
//    std::cout << "Sagital Pelvis Angle: " << DART_DEGREE * getSagitalPelvisAngle() << std::endl;

    // Torso control on sagital plane
    double pelvisSagitalAngle = getSagitalPelvisAngle();
    double tauTorsoSagital
        = -5000.0 * (pelvisSagitalAngle + mDesiredGlobalPelvisAngleOnSagital)
          - 1.0 * (0);
    mTorque[13] = tauTorsoSagital - mTorque[14];

//    cout << "Torque[13]     : " << mTorque[13] << endl;
//    cout << "Torque[14]     : " << mTorque[14] << endl;
//    cout << "tauTorsoSagital: " << tauTorsoSagital << endl;
//    cout << endl;

    // Torso control on coronal plane
    double pelvisCoronalAngle = getCoronalPelvisAngle();
    double tauTorsoCoronal
        = -5000.0 * (pelvisCoronalAngle - mDesiredGlobalPelvisAngleOnCoronal)
          - 1.0 * (0);
    mTorque[10] = -tauTorsoCoronal - mTorque[11];

//    cout << "Torque[10]     : " << mTorque[10] << endl;
//    cout << "Torque[11]     : " << mTorque[11] << endl;
//    cout << "tauTorsoCoronal: " << tauTorsoCoronal << endl;
//    cout << endl;


//    cout << "Stance foot: Left foot" << endl;

  }
  // Stance leg is right leg
  else if (mStanceFoot == mRightFoot)
  {
//    cout << "Stance foot: Right foot" << endl;

    // Torso control on sagital plane
    double pelvisSagitalAngle = getSagitalPelvisAngle();
    double tauTorsoSagital
        = -5000.0 * (pelvisSagitalAngle + mDesiredGlobalPelvisAngleOnSagital)
          - 1.0 * (0);
    mTorque[14] = tauTorsoSagital - mTorque[13];

//    cout << "Torque[13]     : " << mTorque[13] << endl;
//    cout << "Torque[14]     : " << mTorque[14] << endl;
//    cout << "tauTorsoSagital: " << tauTorsoSagital << endl;
//    cout << endl;

    // Torso control on coronal plane
    double pelvisCoronalAngle = getCoronalPelvisAngle();
    double tauTorsoCoronal
        = -5000.0 * (pelvisCoronalAngle - mDesiredGlobalPelvisAngleOnCoronal)
          - 1.0 * (0);
    mTorque[11] = -tauTorsoCoronal - mTorque[10];

    //    cout << "Torque[10]     : " << mTorque[10] << endl;
    //    cout << "Torque[11]     : " << mTorque[11] << endl;
    //    cout << "tauTorsoCoronal: " << tauTorsoCoronal << endl;
    //    cout << endl;
  }
  else
  {
    // No foot is toching the ground
  }
}

//==============================================================================
State* State::getNextState() const
{
  return mNextState;
}

//==============================================================================
double State::getElapsedTime() const
{
  return mElapsedTime;
}

//==============================================================================
void State::setDesiredJointPosition(int _idx, double _val)
{
  assert(0 <= _idx && _idx <= mDesiredJointPositions.size()
         && "Invalid joint index.");

  mDesiredJointPositions[_idx] = _val;
}

//==============================================================================
void State::setDesiredJointPosition(const string& _jointName, double _val)
{
  // TODO(JS)
  NOT_YET(State::setDesiredJointPosition());

  assert(mSkeleton->getJoint(_jointName) != NULL);

  mDesiredJointPositions[mJointMap[_jointName]] = _val;
}

//==============================================================================
double State::getDesiredJointPosition(int _idx) const
{
  assert(0 <= _idx && _idx <= mDesiredJointPositions.size()
         && "Invalid joint index.");

  return mDesiredJointPositions[_idx];
}

//==============================================================================
double State::getDesiredJointPosition(const string& _jointName) const
{
  // TODO(JS)
  NOT_YET(State::getDesiredJointPosition());

  assert(mSkeleton->getJoint(_jointName) != NULL);

  return mDesiredJointPositions[mJointMap.at(_jointName)];
}

//==============================================================================
void State::setDesiredSwingLegGlobalAngleOnSagital(double _val)
{
  mDesiredGlobalSwingLegAngleOnSagital = _val;
}

//==============================================================================
void State::setDesiredSwingLegGlobalAngleOnCoronal(double _val)
{
  mDesiredGlobalSwingLegAngleOnCoronal = _val;
}

//==============================================================================
void State::setDesiredPelvisGlobalAngleOnSagital(double _val)
{
  mDesiredGlobalPelvisAngleOnSagital = _val;
}

//==============================================================================
void State::setDesiredPelvisGlobalAngleOnCoronal(double _val)
{
  mDesiredGlobalPelvisAngleOnCoronal = _val;
}

//==============================================================================
void State::setProportionalGain(int _idx, double _val)
{
  assert(0 <= _idx && _idx <= mKp.size() && "Invalid joint index.");

  mKd[_idx] = _val;
}

//==============================================================================
void State::setProportionalGain(const string& _jointName, double _val)
{
  // TODO(JS)
  NOT_YET(State::setProportionalGain());
}

//==============================================================================
double State::getProportionalGain(int _idx) const
{
  assert(0 <= _idx && _idx <= mKp.size() && "Invalid joint index.");

  return mKp[_idx];
}

//==============================================================================
double State::getProportionalGain(const string& _jointName) const
{
  // TODO(JS)
  NOT_YET(State::getProportionalGain());

  assert(mSkeleton->getJoint(_jointName) != NULL);

  return mKp[mJointMap.at(_jointName)];
}

//==============================================================================
void State::setDerivativeGain(int _idx, double _val)
{
  assert(0 <= _idx && _idx <= mKd.size() && "Invalid joint index.");

  mKd[_idx] = _val;
}

//==============================================================================
void State::setDerivativeGain(const string& _jointName, double _val)
{
  // TODO(JS)
  NOT_YET(State::setDerivativeGain());
}

//==============================================================================
double State::getDerivativeGain(int _idx) const
{
  assert(0 <= _idx && _idx <= mKd.size() && "Invalid joint index.");

  return mKd[_idx];
}

//==============================================================================
//double State::getDerivativeGain(const string& _jointName) const
//{
//  // TODO(JS)
//  NOT_YET(State::getDerivativeGain());
//}

//==============================================================================
void State::setFeedbackSagitalCOMDistance(int _idx, double _val)
{
  assert(0 <= _idx && _idx <= mSagitalCd.size() && "Invalid index.");

  mSagitalCd[_idx] = _val;
}

//==============================================================================
void State::setFeedbackSagitalCOMVelocity(int _idx, double _val)
{
  assert(0 <= _idx && _idx <= mSagitalCv.size() && "Invalid index.");

  mSagitalCv[_idx] = _val;
}

//==============================================================================
void State::setFeedbackCoronalCOMDistance(int _idx, double _val)
{
  assert(0 <= _idx && _idx <= mCoronalCd.size() && "Invalid index.");

  mCoronalCd[_idx] = _val;
}

//==============================================================================
void State::setFeedbackCoronalCOMVelocity(int _idx, double _val)
{
  assert(0 <= _idx && _idx <= mCoronalCv.size() && "Invalid index.");

  mCoronalCv[_idx] = _val;
}

//==============================================================================
void State::setStanceFootToLeftFoot()
{
  mStanceFoot = mLeftFoot;
}

//==============================================================================
void State::setStanceFootToRightFoot()
{
  mStanceFoot = mRightFoot;
}


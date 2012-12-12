// SVN $HeadURL: http://alufr-ros-pkg.googlecode.com/svn/trunk/humanoid_stacks/humanoid_navigation/humanoid_localization/src/HumanoidLocalization.cpp $
// SVN $Id: HumanoidLocalization.cpp 3506 2012-10-22 14:26:10Z hornunga@informatik.uni-freiburg.de $

/*
 * 6D localization for humanoid robots
 *
 * Copyright 2009-2012 Armin Hornung, University of Freiburg
 * http://www.ros.org/wiki/humanoid_localization
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <humanoid_localization/HumanoidLocalization.h>
#include <iostream>

// simple timing benchmark output
#define _BENCH_TIME 0

namespace humanoid_localization{
HumanoidLocalization::HumanoidLocalization(unsigned randomSeed)
:
m_rngEngine(randomSeed),
m_rngNormal(m_rngEngine, NormalDistributionT(0.0, 1.0)),
m_rngUniform(m_rngEngine, UniformDistributionT(0.0, 1.0)),
m_nh(),m_privateNh("~"),
m_odomFrameId("odom"), m_baseFrameId("torso"), m_baseFootprintId("base_footprint"), m_globalFrameId("/map"),
m_useRaycasting(true), m_initFromTruepose(false), m_numParticles(500),
m_numSensorBeams(48),
m_maxOdomInterval(7.0),
m_nEffFactor(1.0), m_minParticleWeight(0.0),
m_bestParticleIdx(-1), m_lastIMUMsgBuffer(5),
m_bestParticleAsMean(true),
m_receivedSensorData(false), m_initialized(false), m_initGlobal(false), m_paused(false),
m_syncedTruepose(false),
m_observationThresholdTrans(0.1), m_observationThresholdRot(M_PI/6),
m_observationThresholdHeadYawRot(0.1), m_observationThresholdHeadPitchRot(0.1),
m_temporalSamplingRange(0.1), m_transformTolerance(0.1),
m_translationSinceScan(0.0), m_rotationSinceScan(0.0),
m_headYawRotationLastScan(0.0), m_headPitchRotationLastScan(0.0),
m_useIMU(false)
{

  // raycasting or endpoint model?
  m_privateNh.param("use_raycasting", m_useRaycasting, m_useRaycasting);

  m_privateNh.param("odom_frame_id", m_odomFrameId, m_odomFrameId);
  m_privateNh.param("base_frame_id", m_baseFrameId, m_baseFrameId);
  m_privateNh.param("base_footprint_id", m_baseFootprintId, m_baseFootprintId);
  m_privateNh.param("global_frame_id", m_globalFrameId, m_globalFrameId);
  m_privateNh.param("init_from_truepose", m_initFromTruepose, m_initFromTruepose);
  m_privateNh.param("init_global", m_initGlobal, m_initGlobal);
  m_privateNh.param("best_particle_as_mean", m_bestParticleAsMean, m_bestParticleAsMean);
  m_privateNh.param("sync_truepose", m_syncedTruepose, m_syncedTruepose);
  m_privateNh.param("num_particles", m_numParticles, m_numParticles);
  m_privateNh.param("max_odom_interval", m_maxOdomInterval, m_maxOdomInterval);
  m_privateNh.param("neff_factor", m_nEffFactor, m_nEffFactor);
  m_privateNh.param("min_particle_weight", m_minParticleWeight, m_minParticleWeight);

  m_privateNh.param("initial_pose/x", m_initPose(0), 0.0);
  m_privateNh.param("initial_pose/y", m_initPose(1), 0.0);
  m_privateNh.param("initial_pose/z", m_initPose(2), 0.32); // hip height when standing
  m_privateNh.param("initial_pose/roll", m_initPose(3), 0.0);
  m_privateNh.param("initial_pose/pitch", m_initPose(4), 0.0);
  m_privateNh.param("initial_pose/yaw", m_initPose(5), 0.0);
  m_privateNh.param("initial_pose_real_zrp", m_initPoseRealZRP, false);

  m_privateNh.param("initial_std/x", m_initNoiseStd(0), 0.1); // 0.1
  m_privateNh.param("initial_std/y", m_initNoiseStd(1), 0.1); // 0.1
  m_privateNh.param("initial_std/z", m_initNoiseStd(2), 0.02); // 0.02
  m_privateNh.param("initial_std/roll", m_initNoiseStd(3), 0.04); // 0.04
  m_privateNh.param("initial_std/pitch", m_initNoiseStd(4), 0.04); // 0.04
  m_privateNh.param("initial_std_yaw", m_initNoiseStd(5), M_PI/12); // M_PI/12

  // laser observation model parameters:
  m_privateNh.param("num_sensor_beams", m_numSensorBeams, m_numSensorBeams);
  m_privateNh.param("max_range", m_filterMaxRange, 30.0);
  m_privateNh.param("min_range", m_filterMinRange, 0.05);
  ROS_DEBUG("Using a range filter of %f to %f", m_filterMinRange, m_filterMaxRange);

  m_privateNh.param("update_min_trans", m_observationThresholdTrans, m_observationThresholdTrans);
  m_privateNh.param("update_min_rot", m_observationThresholdRot, m_observationThresholdRot);
  m_privateNh.param("update_min_head_yaw", m_observationThresholdHeadYawRot, m_observationThresholdHeadYawRot);
  m_privateNh.param("update_min_head_pitch", m_observationThresholdHeadPitchRot, m_observationThresholdHeadPitchRot);
  m_privateNh.param("temporal_sampling_range", m_temporalSamplingRange, m_temporalSamplingRange);
  m_privateNh.param("transform_tolerance", m_transformTolerance, m_transformTolerance);

  m_privateNh.param("use_imu", m_useIMU, m_useIMU);

  m_motionModel = boost::shared_ptr<MotionModel>(new MotionModel(&m_privateNh, &m_rngEngine, &m_tfListener, m_odomFrameId, m_baseFrameId));

  if (m_useRaycasting){
    m_mapModel = boost::shared_ptr<MapModel>(new OccupancyMap(&m_privateNh));
    m_observationModel = boost::shared_ptr<ObservationModel>(new RaycastingModel(&m_privateNh, m_mapModel, &m_rngEngine));
  } else{
#ifndef SKIP_ENDPOINT_MODEL
    //m_mapModel = boost::shared_ptr<MapModel>(new DistanceMap(&m_privateNh));
    m_mapModel = boost::shared_ptr<MapModel>(new OccupancyMap(&m_privateNh));
    m_observationModel = boost::shared_ptr<ObservationModel>(new EndpointModel(&m_privateNh, m_mapModel, &m_rngEngine));
#else
    ROS_FATAL("EndpointModel not compiled due to missing dynamicEDT3D");
    exit(-1);
#endif
  }


  m_particles.resize(m_numParticles);
  m_poseArray.poses.resize(m_numParticles);
  m_poseArray.header.frame_id = m_globalFrameId;
  m_tfListener.clear();


  // publishers can be advertised first, before needed:
  m_posePub = m_nh.advertise<geometry_msgs::PoseWithCovarianceStamped>("pose", 10);
  m_poseEvalPub = m_nh.advertise<geometry_msgs::PoseWithCovarianceStamped>("pose_eval", 10);
  m_poseOdomPub = m_privateNh.advertise<geometry_msgs::PoseStamped>("pose_odom_sync", 10);
  m_poseTruePub = m_privateNh.advertise<geometry_msgs::PoseStamped>("pose_true_sync", 10);
  m_poseArrayPub = m_privateNh.advertise<geometry_msgs::PoseArray>("particlecloud", 10);
  m_bestPosePub = m_privateNh.advertise<geometry_msgs::PoseArray>("best_particle", 10);
  m_nEffPub = m_privateNh.advertise<std_msgs::Float32>("n_eff", 10);
  m_filteredPointCloudPub = m_privateNh.advertise<sensor_msgs::PointCloud2>("filtered_cloud", 1);


  reset();

  // ROS subscriptions last:
  m_globalLocSrv = m_nh.advertiseService("global_localization", &HumanoidLocalization::globalLocalizationCallback, this);

  // subscription on laser, tf message filter
  m_laserSub = new message_filters::Subscriber<sensor_msgs::LaserScan>(m_nh, "scan", 100);
  m_laserFilter = new tf::MessageFilter<sensor_msgs::LaserScan>(*m_laserSub, m_tfListener, m_odomFrameId, 100);
  m_laserFilter->registerCallback(boost::bind(&HumanoidLocalization::laserCallback, this, _1));

  // subscription on point cloud, tf message filter
  m_pointCloudSub = new message_filters::Subscriber<sensor_msgs::PointCloud2 >(m_nh, "point_cloud", 100);
  m_pointCloudFilter = new tf::MessageFilter<sensor_msgs::PointCloud2 >(*m_pointCloudSub, m_tfListener, m_odomFrameId, 100);
  m_pointCloudFilter->registerCallback(boost::bind(&HumanoidLocalization::pointCloudCallback, this, _1));

  // subscription on init pose, tf message filter
  m_initPoseSub = new message_filters::Subscriber<geometry_msgs::PoseWithCovarianceStamped>(m_nh, "initialpose", 2);
  m_initPoseFilter = new tf::MessageFilter<geometry_msgs::PoseWithCovarianceStamped>(*m_initPoseSub, m_tfListener, m_globalFrameId, 2);
  m_initPoseFilter->registerCallback(boost::bind(&HumanoidLocalization::initPoseCallback, this, _1));


  m_pauseIntegrationSub = m_privateNh.subscribe("pause_localization", 1, &HumanoidLocalization::pauseLocalizationCallback, this);
  m_pauseLocSrv = m_privateNh.advertiseService("pause_localization_srv", &HumanoidLocalization::pauseLocalizationSrvCallback, this);
  m_resumeLocSrv = m_privateNh.advertiseService("resume_localization_srv", &HumanoidLocalization::resumeLocalizationSrvCallback, this);

  m_imuSub = m_nh.subscribe("imu", 5, &HumanoidLocalization::imuCallback, this);

  ROS_INFO("NaoLocalization initialized with %d particles.", m_numParticles);
}

HumanoidLocalization::~HumanoidLocalization() {

  delete m_laserFilter;
  delete m_laserSub;

  delete m_pointCloudFilter;
  delete m_pointCloudSub;

  delete m_initPoseFilter;
  delete m_initPoseSub;

}

void HumanoidLocalization::reset(){

#if defined(_BENCH_TIME)
  ros::WallTime startTime = ros::WallTime::now();
#endif

  if (m_initGlobal){
    this->initGlobal();
  } else {
    geometry_msgs::PoseWithCovarianceStampedPtr posePtr(new geometry_msgs::PoseWithCovarianceStamped());

    if (m_initFromTruepose){ // useful for evaluation, when ground truth available:
      geometry_msgs::PoseStamped truePose;
      tf::Stamped<tf::Pose> truePoseTF;
      tf::Stamped<tf::Pose> ident (tf::Transform(tf::createIdentityQuaternion(), btVector3(0,0,0)), ros::Time::now(), "/torso_real"); // TODO: param

      ros::Time lookupTime = ros::Time::now();
      while(m_nh.ok() && !m_tfListener.waitForTransform(m_globalFrameId, ident.frame_id_, lookupTime, ros::Duration(1.0))){
        ROS_WARN("Waiting for Truepose transform for initialization failed, trying again...");
        lookupTime = ros::Time::now();
      }
      ident.stamp_ = lookupTime;

      m_tfListener.transformPose(m_globalFrameId, ident, truePoseTF);
      tf::poseStampedTFToMsg(truePoseTF, truePose);
      tf::poseTFToMsg(truePoseTF, posePtr->pose.pose);
      posePtr->header = truePose.header;


      // initial covariance acc. to params
      for(int j=0; j < 6; ++j){
        for (int i = 0; i < 6; ++i){
          if (i == j)
            posePtr->pose.covariance.at(i*6 +j) = m_initNoiseStd(i) * m_initNoiseStd(i);
          else
            posePtr->pose.covariance.at(i*6 +j) = 0.0;
        }
      }

    } else{
      posePtr.reset(new geometry_msgs::PoseWithCovarianceStamped());
      for (int i = 0; i < 6; ++i){
        posePtr->pose.covariance.at(i*6 +i) = m_initNoiseStd(i) * m_initNoiseStd(i);
      }

      posePtr->pose.pose.position.x = m_initPose(0);
      posePtr->pose.pose.position.y = m_initPose(1);
      double roll, pitch;
      if(m_initPoseRealZRP) {
        // Get latest pose height
        tf::Stamped<tf::Pose> lastOdomPose;
        double poseHeight;
        if(m_motionModel->getLastOdomPose(lastOdomPose) &&
            lookupPoseHeight(lastOdomPose.stamp_, poseHeight)) {
          posePtr->pose.pose.position.z = poseHeight;
        } else {
          ROS_WARN("Could not determine current pose height, falling back to init_pose_z");
          posePtr->pose.pose.position.z = m_initPose(2);
        }

        // Get latest roll and pitch
        if(!m_lastIMUMsgBuffer.empty()) {
          getRPY(m_lastIMUMsgBuffer.back().orientation, roll, pitch);
        } else {
          ROS_WARN("Could not determine current roll and pitch, falling back to init_pose_{roll,pitch}");
          roll = m_initPose(3);
          pitch = m_initPose(4);
        }
      } else {
        // Use pose height, roll and pitch from init_pose_{z,roll,pitch} parameters
        posePtr->pose.pose.position.z = m_initPose(2);
        roll = m_initPose(3);
        pitch = m_initPose(4);
      }

      tf::Quaternion quat;
      quat.setRPY(roll, pitch, m_initPose(5));
      tf::quaternionTFToMsg(quat, posePtr->pose.pose.orientation);

    }

    this->initPoseCallback(posePtr);

  }





#if defined(_BENCH_TIME)
  double dt = (ros::WallTime::now() - startTime).toSec();
  ROS_INFO_STREAM("Initialization of "<< m_numParticles << " particles took "
                  << dt << "s (="<<dt/m_numParticles<<"s/particle)");
#endif


}


void HumanoidLocalization::laserCallback(const sensor_msgs::LaserScanConstPtr& msg){
  ROS_DEBUG("Laser received (time: %f)", msg->header.stamp.toSec());
  
  if (!m_initialized){
    ROS_WARN("Loclization not initialized yet, skipping laser callback.");
    return;
  }

  double timediff = (msg->header.stamp - m_lastLaserTime).toSec();
  if (m_receivedSensorData && timediff < timediff){
    ROS_WARN("Ignoring received laser data that is %f s older than previous data!", timediff);
    return;
  }
  

  /// absolute, current odom pose
  tf::Stamped<tf::Pose> odomPose;
  // check if odometry available, skip scan if not.
  if (!m_motionModel->lookupOdomPose(msg->header.stamp, odomPose))
    return;

  // relative odom transform to last odomPose
  tf::Transform odomTransform = m_motionModel->computeOdomTransform(odomPose);

  bool sensor_integrated = false;
  if (!m_paused && (!m_receivedSensorData || isAboveMotionThreshold(odomTransform))) {
    
    // convert laser to point cloud first:
    PointCloud pc_filtered;
    std::vector<float> laserRangesSparse;
    prepareLaserPointCloud(msg, pc_filtered, laserRangesSparse);
    
    sensor_integrated = localizeWithMeasurement(pc_filtered, laserRangesSparse, msg->range_max);
    
  } else{ // no observation necessary: propagate particles forward by full interval

    m_motionModel->applyOdomTransform(m_particles, odomTransform);
  }

  m_motionModel->storeOdomPose(odomPose);
  publishPoseEstimate(msg->header.stamp, sensor_integrated);
  m_lastLaserTime = msg->header.stamp;
}

bool HumanoidLocalization::isAboveMotionThreshold(const tf::Transform& odomTransform){
  float length = odomTransform.getOrigin().length();
  if (length > 0.1){
    ROS_WARN("Length of odometry change unexpectedly high: %f", length);
  }

  m_translationSinceScan += length;
  double yaw, pitch, roll;
  odomTransform.getBasis().getRPY(roll, pitch, yaw);
  if (std::abs(yaw) > 0.15){
    ROS_WARN("Yaw of odometry change unexpectedly high: %f", yaw);
  }
  m_rotationSinceScan += std::abs(yaw);


  return (m_translationSinceScan >= m_observationThresholdTrans
      || m_rotationSinceScan >= m_observationThresholdRot);
}

bool HumanoidLocalization::localizeWithMeasurement(const PointCloud& pc_filtered, const std::vector<float>& ranges, double max_range){
  ros::WallTime startTime = ros::WallTime::now();
  ros::Time t = pc_filtered.header.stamp;
  // apply motion model with temporal sampling:
  m_motionModel->applyOdomTransformTemporal(m_particles, t, m_temporalSamplingRange);
  
  // transformation from torso frame to laser
  // this takes the latest tf, assumes that torso to laser did not change over temp. sampling!
  tf::StampedTransform localSensorFrame;

  if (!m_motionModel->lookupLocalTransform(pc_filtered.header.frame_id, t, localSensorFrame))
    return false;
  tf::Transform torsoToSensor(localSensorFrame.inverse());
  
//### Particles in log-form from here...
  toLogForm();
  // integrated pose (z, roll, pitch) meas. only if data OK:
  bool imuMsgOk = false;
  double angleX, angleY;
  if(m_useIMU) {
    ros::Time imuStamp;
    imuMsgOk = getImuMsg(t, imuStamp, angleX, angleY);
  } else {
    tf::Stamped<tf::Pose> lastOdomPose;
    if(m_motionModel->lookupOdomPose(t, lastOdomPose)) {
      double dropyaw;
      lastOdomPose.getBasis().getRPY(angleX, angleY, dropyaw);
      imuMsgOk = true;
    }
  }

  tf::StampedTransform footprintToTorso;
  if(imuMsgOk) {
    if (!m_motionModel->lookupLocalTransform(m_baseFootprintId, t, footprintToTorso)) {
      ROS_WARN("Could not obtain pose height in localization, skipping Pose integration");
    } else {
      m_observationModel->integratePoseMeasurement(m_particles, angleX, angleY, footprintToTorso);
    }
  } else {
    ROS_WARN("Could not obtain roll and pitch measurement, skipping Pose integration");
  }

  m_filteredPointCloudPub.publish(pc_filtered);

  m_observationModel->integrateMeasurement(m_particles, pc_filtered, ranges, max_range, torsoToSensor);

  // TODO: verify poses before measurements, ignore particles then
  m_mapModel->verifyPoses(m_particles);

  // normalize weights and transform back from log:
  normalizeWeights();
  //### Particles back in regular form now

  double nEffParticles = nEff();

  std_msgs::Float32 nEffMsg;
  nEffMsg.data = nEffParticles;
  m_nEffPub.publish(nEffMsg);

  if (nEffParticles <= m_nEffFactor*m_particles.size()){ // selective resampling
    ROS_INFO("Resampling, nEff=%f, numParticles=%zd", nEffParticles, m_particles.size());
    resample();
  } else {
    ROS_INFO("Skipped resampling, nEff=%f, numParticles=%zd", nEffParticles, m_particles.size());
  }

  m_receivedSensorData = true;
  m_rotationSinceScan = 0.0;
  m_translationSinceScan = 0.0;

  double dt = (ros::WallTime::now() - startTime).toSec();
  ROS_INFO_STREAM("Observations for "<< m_numParticles << " particles took "
                  << dt << "s (="<<dt/m_numParticles<<"s/particle)");

  return true;
}

void HumanoidLocalization::prepareLaserPointCloud(const sensor_msgs::LaserScanConstPtr& laser, PointCloud& pc, std::vector<float>& ranges) const{
  // prepare laser message:
  unsigned numBeams = laser->ranges.size();
  unsigned step = computeBeamStep(numBeams);

  unsigned int numBeamsSkipped = 0;

  // range_min of laser is also used to filter out wrong messages:
  double laserMin = std::max(double(laser->range_min), m_filterMinRange);

  // (range_max readings stay, will be used in the sensor model)

  ranges.reserve(m_numSensorBeams+3);

  // build a point cloud
  pc.header = laser->header;
  pc.points.reserve(m_numSensorBeams+3);
  for (unsigned beam_idx = 0; beam_idx < numBeams; beam_idx+= step){
    float range = laser->ranges[beam_idx];
    if (range >= laserMin && range <= m_filterMaxRange){
      double laserAngle = laser->angle_min + beam_idx * laser->angle_increment;
      tf::Transform laserAngleRotation(tf::Quaternion(tf::Vector3(0.0, 0.0, 1.0), laserAngle));
      tf::Vector3 laserEndpointTrans(range, 0.0, 0.0);
      tf::Vector3 pt(laserAngleRotation * laserEndpointTrans);

      pc.points.push_back(pcl::PointXYZ(pt.x(), pt.y(), pt.z()));
      ranges.push_back(range);

    } else{
      numBeamsSkipped++;
    }

  }
  pc.height = 1;
  pc.width = pc.points.size();
  pc.is_dense = true;
  ROS_INFO("%u/%zu laser beams skipped (out of valid range)", numBeamsSkipped, ranges.size());
}

void HumanoidLocalization::prepareFilteredPointCloud(const sensor_msgs::PointCloud2ConstPtr& pcIn, PointCloud& pc, std::vector<float>& ranges) const{
  // prepare point cloud:
  unsigned numBeams = pcIn->data.size();
  //unsigned step = computeBeamStep(numBeams);

  unsigned int numBeamsSkipped = 0;

  // range_min of laser is also used to filter out wrong messages:
  //double laserMin = std::max(double(laser->range_min), m_filterMinRange);
  double sensorMin = m_filterMinRange;

  // (range_max readings stay, will be used in the sensor model)

  //ranges.reserve(m_numSensorBeams+3);
  unsigned step = (numBeams/m_numSensorBeams);
  unsigned jump = m_numSensorBeams;
  ranges.reserve(numBeams);

  /*
  // build a point cloud
  pc.header = laser->header;
  pc.points.reserve(m_numSensorBeams+3);
  for (unsigned beam_idx = 0; beam_idx < numBeams; beam_idx+= step){
    float range = laser->ranges[beam_idx];
    if (range >= laserMin && range <= m_filterMaxRange){
      double laserAngle = laser->angle_min + beam_idx * laser->angle_increment;
      tf::Transform laserAngleRotation(tf::Quaternion(tf::Vector3(0.0, 0.0, 1.0), laserAngle));
      tf::Vector3 laserEndpointTrans(range, 0.0, 0.0);
      tf::Vector3 pt(laserAngleRotation * laserEndpointTrans);

      pc.points.push_back(pcl::PointXYZ(pt.x(), pt.y(), pt.z()));
      ranges.push_back(range);

    } else{
      numBeamsSkipped++;
    }

  }
  */
  pc.header = pcIn->header;
  pc.points.reserve(numBeams);
  PointCloud pc_trans;
  pcl::fromROSMsg(*pcIn, pc_trans);
  PointCloud::const_iterator pc_it = pc_trans.begin();

  std::vector<float>::const_iterator ranges_it = ranges.begin();
  int previous_idx = 0;
  for ( ; pc_it != pc_trans.end(); ++pc_it){
    //for(int i = 0; i < 240; i=i+jump) {
    //for(int j = 0; j < 320; j=j+jump) {
      //step = i*320 + j - previous_idx;
      //ROS_INFO("Step %d i %d j %d i*j+j", step, i, j, i*640+j);
      //pc_it = pc_it + step;
      //previous_idx = i*320 + j;
      //++pc_it;
      if (isnan(pc_it->x) || isnan(pc_it->y) || isnan(pc_it->z)) {
        numBeamsSkipped++;
      } else {
        pc.points.push_back(pcl::PointXYZ(pc_it->x , pc_it->y, pc_it->z));
        ranges.push_back(sqrt((pc_it->x * pc_it->x) + (pc_it->y * pc_it->y) + (pc_it->z * pc_it->z)));
        //ROS_INFO("PC coords: x=%f, y=%f, z=%f", pc_it->x , pc_it->y, pc_it->z);
      }
   // }
  }
  pc.height = 1;
  pc.width = pc.points.size();
  pc.is_dense = true;
  ROS_INFO("%u/%zu laser beams skipped (out of valid range)", numBeamsSkipped, ranges.size());
}

unsigned HumanoidLocalization::computeBeamStep(unsigned numBeams) const{
  unsigned step = 1;
  if (m_numSensorBeams > 1){
    // from "amcl_node"
    step = (numBeams -1) / (m_numSensorBeams - 1);
    if (step < 1)
      step = 1;
  } else if (m_numSensorBeams == 1){
    step = numBeams;
  }

  return step;
}

void HumanoidLocalization::pointCloudCallback(const sensor_msgs::PointCloud2ConstPtr& msg) {
//ROS_ERROR("Point cloud callback still needs to be integrated.");
  ROS_DEBUG("Point cloud received (time: %f)", msg->header.stamp.toSec());

  if (!m_initialized){
    ROS_WARN("Localization not initialized yet, skipping point cloud callback.");
    return;
  }

  double timediff = (msg->header.stamp - m_lastPointCloudTime).toSec();
  if (m_receivedSensorData && timediff < timediff){
    ROS_WARN("Ignoring received point cloud data that is %f s older than previous data!", timediff);
    return;
  }


  /// absolute, current odom pose
  tf::Stamped<tf::Pose> odomPose;
  // check if odometry available, skip scan if not.
  if (!m_motionModel->lookupOdomPose(msg->header.stamp, odomPose))
    return;

  // relative odom transform to last odomPose
  tf::Transform odomTransform = m_motionModel->computeOdomTransform(odomPose);

  bool sensor_integrated = false;
  if (!m_paused && (!m_receivedSensorData || isAboveMotionThreshold(odomTransform))) {

    // convert laser to point cloud first:
    PointCloud pc_filtered;
    std::vector<float> pointCloudRangesSparse;
    prepareFilteredPointCloud(msg, pc_filtered, pointCloudRangesSparse);

    sensor_integrated = localizeWithMeasurement(pc_filtered, pointCloudRangesSparse, 10.0);

  } else{ // no observation necessary: propagate particles forward by full interval

    m_motionModel->applyOdomTransform(m_particles, odomTransform);
  }

  m_motionModel->storeOdomPose(odomPose);
  publishPoseEstimate(msg->header.stamp, sensor_integrated);
  m_lastPointCloudTime = msg->header.stamp;
}

void HumanoidLocalization::imuCallback(const sensor_msgs::ImuConstPtr& msg){
  m_lastIMUMsgBuffer.push_back(*msg);
}

bool HumanoidLocalization::getImuMsg(const ros::Time& stamp, ros::Time& imuStamp, double& angleX, double& angleY) const {
  if(m_lastIMUMsgBuffer.empty())
    return false;

  typedef boost::circular_buffer<sensor_msgs::Imu>::const_iterator ItT;
  const double maxAge = 0.2;
  double closestOlderStamp = std::numeric_limits<double>::max();
  double closestNewerStamp = std::numeric_limits<double>::max();
  ItT closestOlder = m_lastIMUMsgBuffer.end(), closestNewer = m_lastIMUMsgBuffer.end();
  for(ItT it = m_lastIMUMsgBuffer.begin(); it != m_lastIMUMsgBuffer.end(); it++) {
    const double age = (stamp - it->header.stamp).toSec();
    if(age >= 0.0 && age < closestOlderStamp) {
      closestOlderStamp = age;
      closestOlder = it;
    } else if(age < 0.0 && -age < closestNewerStamp) {
      closestNewerStamp = -age;
      closestNewer = it;
    }
  }

  if(closestOlderStamp < maxAge && closestNewerStamp < maxAge && closestOlderStamp + closestNewerStamp > 0.0) {
    // Linear interpolation
    const double weightOlder = closestNewerStamp / (closestNewerStamp + closestOlderStamp);
    const double weightNewer = 1.0 - weightOlder;
    imuStamp = ros::Time(weightOlder * closestOlder->header.stamp.toSec()
                          + weightNewer * closestNewer->header.stamp.toSec());
    double olderX, olderY, newerX, newerY;
    getRPY(closestOlder->orientation, olderX, olderY);
    getRPY(closestNewer->orientation, newerX, newerY);
    angleX   = weightOlder * olderX  + weightNewer * newerX;
    angleY   = weightOlder * olderY + weightNewer * newerY;
    ROS_DEBUG("Msg: %.3f, Interpolate [%.3f .. %.3f .. %.3f]\n", stamp.toSec(), closestOlder->header.stamp.toSec(),
              imuStamp.toSec(), closestNewer->header.stamp.toSec());
    return true;
  } else if(closestOlderStamp < maxAge || closestNewerStamp < maxAge) {
    // Return closer one
    ItT it = (closestOlderStamp < closestNewerStamp) ? closestOlder : closestNewer;
    imuStamp = it->header.stamp;
    getRPY(it->orientation, angleX, angleY);
    return true;
  } else {
    if(closestOlderStamp < closestNewerStamp)
      ROS_WARN("Closest IMU message is %.2f seconds too old, skipping pose integration", closestOlderStamp);
    else
      ROS_WARN("Closest IMU message is %.2f seconds too new, skipping pose integration", closestNewerStamp);
    return false;
  }
}

void HumanoidLocalization::initPoseCallback(const geometry_msgs::PoseWithCovarianceStampedConstPtr& msg){
  tf::Pose pose;
  tf::poseMsgToTF(msg->pose.pose, pose);

  if (msg->header.frame_id != m_globalFrameId){
    ROS_WARN("Frame ID of \"initialpose\" (%s) is different from the global frame %s", msg->header.frame_id.c_str(), m_globalFrameId.c_str());
  }

  std::vector<double> heights;
  double poseHeight = 0.0;
  if (std::abs(pose.getOrigin().getZ()) < 0.01){
    m_mapModel->getHeightlist(pose.getOrigin().getX(), pose.getOrigin().getY(), 0.6, heights);
    if (heights.size() == 0){
      ROS_WARN("No ground level to stand on found at map position, assuming 0");
      heights.push_back(0.0);
    }

    bool poseHeightOk = false;
    if(m_initPoseRealZRP) {
      ros::Time stamp(msg->header.stamp);
      if(stamp.isZero()) {
        // Header stamp is not set (e.g. RViz), use stamp from latest pose message instead
        tf::Stamped<tf::Pose> lastOdomPose;
        m_motionModel->getLastOdomPose(lastOdomPose);
        stamp = lastOdomPose.stamp_;
      }
      poseHeightOk = lookupPoseHeight(stamp, poseHeight);
      if(!poseHeightOk) {
        ROS_WARN("Could not determine current pose height, falling back to init_pose_z");
      }
    }
    if(!poseHeightOk) {
      ROS_INFO("Use pose height from init_pose_z");
      poseHeight = m_initPose(2);
    }
  }


  Matrix6d initCov;
  if ((std::abs(msg->pose.covariance.at(6*0+0) - 0.25) < 0.1) && (std::abs(msg->pose.covariance.at(6*1+1) -0.25) < 0.1)
      && (std::abs(msg->pose.covariance.at(6*3+3) - M_PI/12.0 * M_PI/12.0)<0.1)){
    ROS_INFO("Covariance originates from RViz, using default parameters instead");
    initCov = Matrix6d::Zero();
    initCov.diagonal() = m_initNoiseStd.cwiseProduct(m_initNoiseStd);

    // manually set r&p, rviz values are 0
    bool ok = false;
    const double yaw = tf::getYaw(pose.getRotation());
    if(m_initPoseRealZRP) {
      bool useOdometry = true;
      if(m_useIMU) {
        if(m_lastIMUMsgBuffer.empty()) {
          ROS_WARN("Could not determine current roll and pitch because IMU message buffer is empty.");
        } else {
          double roll, pitch;
          if(msg->header.stamp.isZero()) {
            // Header stamp is not set (e.g. RViz), use stamp from latest IMU message instead
            getRPY(m_lastIMUMsgBuffer.back().orientation, roll, pitch);
            ok = true;
          } else {
            ros::Time imuStamp;
            ok = getImuMsg(msg->header.stamp, imuStamp, roll, pitch);
          }
          if(ok) {
            ROS_INFO("roll and pitch not set in initPoseCallback, use IMU values (roll = %f, pitch = %f) instead", roll, pitch);
            pose.setRotation(tf::createQuaternionFromRPY(roll, pitch, yaw));
            useOdometry = false;
          } else {
            ROS_WARN("Could not determine current roll and pitch from IMU, falling back to odometry roll and pitch");
            useOdometry = true;
          }
        }
      }

      if(useOdometry) {
        double roll, pitch, dropyaw;
        tf::Stamped<tf::Pose> lastOdomPose;
        ok = m_motionModel->getLastOdomPose(lastOdomPose);
        if(ok) {
          lastOdomPose.getBasis().getRPY(roll, pitch, dropyaw);
          pose.setRotation(tf::createQuaternionFromRPY(roll, pitch, yaw));
          ROS_INFO("roll and pitch not set in initPoseCallback, use odometry values (roll = %f, pitch = %f) instead", roll, pitch);
        } else {
          ROS_WARN("Could not determine current roll and pitch from odometry, falling back to init_pose_{roll,pitch} parameters");
        }
      }
    }

    if(!ok) {
      ROS_INFO("roll and pitch not set in initPoseCallback, use init_pose_{roll,pitch} parameters instead");
      pose.setRotation(tf::createQuaternionFromRPY(m_initPose(3), m_initPose(4), yaw));
    }
  } else{
    for(int j=0; j < initCov.cols(); ++j){
      for (int i = 0; i < initCov.rows(); ++i){
        initCov(i,j) = msg->pose.covariance.at(i*initCov.cols() +j);
      }
    }
  }

  // sample from intial pose covariance:
  Matrix6d initCovL = initCov.llt().matrixL();
  tf::Transform transformNoise; // transformation on original pose from noise
  unsigned idx = 0;
  for(Particles::iterator it = m_particles.begin(); it != m_particles.end(); ++it){
    Vector6d poseNoise;
    for (unsigned i = 0; i < 6; ++i){
      poseNoise(i) = m_rngNormal();
    }
    Vector6d poseCovNoise = initCovL * poseNoise; // is now drawn according to covariance noise
    // if a variance is set to 0 => no noise!
    for (unsigned i = 0; i < 6; ++i){
      if (std::abs(initCov(i,i)) < 0.00001)
        poseCovNoise(i) = 0.0;
    }


    transformNoise.setOrigin(tf::Vector3(poseCovNoise(0), poseCovNoise(1), poseCovNoise(2)));
    tf::Quaternion q;
    q.setRPY(poseCovNoise(3), poseCovNoise(4),poseCovNoise(5));

    transformNoise.setRotation(q);
    it->pose = pose;

    if (heights.size() > 0){
      // distribute particles evenly between levels:
      it->pose.getOrigin().setZ(heights.at(int(double(idx)/m_particles.size() * heights.size())) + poseHeight);
    }

    it->pose *= transformNoise;

    it->weight = 1.0/m_particles.size();
    idx++;
  }

  ROS_INFO("Pose reset around mean (%f %f %f)", pose.getOrigin().getX(), pose.getOrigin().getY(), pose.getOrigin().getZ());

  // reset internal state:
  m_motionModel->reset();
  m_translationSinceScan = 0.0;
  m_rotationSinceScan = 0.0;
  m_receivedSensorData = false;
  m_initialized = true;

  publishPoseEstimate(msg->header.stamp, false);
}

bool HumanoidLocalization::globalLocalizationCallback(std_srvs::Empty::Request& req, std_srvs::Empty::Response& res)
{

  initGlobal();

  return true;
}

void HumanoidLocalization::normalizeWeights() {

  double wmin = std::numeric_limits<double>::max();
  double wmax = -std::numeric_limits<double>::max();

  for (unsigned i=0; i < m_particles.size(); ++i){
    double weight = m_particles[i].weight;
    assert (!isnan(weight));
    if (weight < wmin)
      wmin = weight;
    if (weight > wmax){
      wmax = weight;
      m_bestParticleIdx = i;
    }
  }
  if (wmin > wmax){
    ROS_ERROR_STREAM("Error in weights: min=" << wmin <<", max="<<wmax<<", 1st particle weight="<< m_particles[1].weight<< std::endl);

  }

  double min_normalized_value;
  if (m_minParticleWeight > 0.0)
    min_normalized_value = std::max(log(m_minParticleWeight), wmin - wmax);
  else
    min_normalized_value = wmin - wmax;

  double max_normalized_value = 0.0; // = log(1.0);
  double dn = max_normalized_value-min_normalized_value;
  double dw = wmax-wmin;
  if (dw == 0.0) dw = 1;
  double scale = dn/dw;
  if (scale < 0.0){
    ROS_WARN("normalizeWeights: scale is %f < 0, dw=%f, dn=%f", scale, dw, dn );
  }
  double offset = -wmax*scale;
  double weights_sum = 0.0;

#pragma omp parallel
  {

#pragma omp for
    for (unsigned i = 0; i < m_particles.size(); ++i){
      double w = m_particles[i].weight;
      w = exp(scale*w+offset);
      assert(!isnan(w));
      m_particles[i].weight = w;
#pragma omp atomic
      weights_sum += w;
    }

    assert(weights_sum > 0.0);
    // normalize sum to 1:
#pragma omp for
    for (unsigned i = 0; i < m_particles.size(); ++i){
      m_particles[i].weight /= weights_sum;
    }

  }
}

double HumanoidLocalization::getCumParticleWeight() const{
  double cumWeight=0.0;

  //compute the cumulative weights
  for (Particles::const_iterator it = m_particles.begin(); it != m_particles.end(); ++it){
    cumWeight += it->weight;
  }

  return cumWeight;
}

void HumanoidLocalization::resample(unsigned numParticles){

  if (numParticles <= 0)
    numParticles = m_numParticles;

  //compute the interval
  double interval=getCumParticleWeight()/numParticles;

  //compute the initial target weight
  double target=interval*m_rngUniform();

  //compute the resampled indexes
  double cumWeight=0;
  std::vector<unsigned> indices(numParticles);

  unsigned n=0;
  for (unsigned i = 0; i < m_particles.size(); ++i){
    cumWeight += m_particles[i].weight;
    while(cumWeight > target && n < numParticles){
      if (m_bestParticleIdx >= 0 && i == unsigned(m_bestParticleIdx)){
        m_bestParticleIdx = n;
      }

      indices[n++]=i;
      target+=interval;
    }
  }
  // indices now contains the indices to draw from the particles distribution

  Particles oldParticles = m_particles;
  m_particles.resize(numParticles);
  m_poseArray.poses.resize(numParticles);
  double newWeight = 1.0/numParticles;

  for (unsigned i = 0; i < numParticles; ++i){
    m_particles[i].pose = oldParticles[indices[i]].pose;
    m_particles[i].weight = newWeight;
  }
}

void HumanoidLocalization::initGlobal(){
  ROS_INFO("Initializing with uniform distribution");

  m_mapModel->initGlobal(m_particles, m_initPose, m_initNoiseStd, m_rngUniform, m_rngNormal);


  ROS_INFO("Global localization done");
  m_motionModel->reset();
  m_translationSinceScan = 0.0;
  m_rotationSinceScan = 0.0;
  m_receivedSensorData = false;
  m_initialized = true;

  publishPoseEstimate(ros::Time::now(), false);

}

void HumanoidLocalization::publishPoseEstimate(const ros::Time& time, bool publish_eval){

  ////
  // send all hypotheses as arrows:
  ////

  m_poseArray.header.stamp = time;

  if (m_poseArray.poses.size() != m_particles.size())
    m_poseArray.poses.resize(m_particles.size());

  for (unsigned i = 0; i < m_particles.size(); ++i){
    tf::poseTFToMsg(m_particles[i].pose, m_poseArray.poses[i]);
  }

  m_poseArrayPub.publish(m_poseArray);

  ////
  // send best particle as pose and one array:
  ////
  geometry_msgs::PoseWithCovarianceStamped p;
  p.header.stamp = time;
  p.header.frame_id = m_globalFrameId;

  tf::Pose bestParticlePose;
  if (m_bestParticleAsMean)
    bestParticlePose = getMeanParticlePose();
  else
    bestParticlePose = getBestParticlePose();

  tf::poseTFToMsg(bestParticlePose,p.pose.pose);
  m_posePub.publish(p);

  if (publish_eval){
    m_poseEvalPub.publish(p);
  }

  geometry_msgs::PoseArray bestPose;
  bestPose.header = p.header;
  bestPose.poses.resize(1);
  tf::poseTFToMsg(bestParticlePose, bestPose.poses[0]);
  m_bestPosePub.publish(bestPose);

  ////
  // send incremental odom pose (synced to localization)
  ////
  tf::Stamped<tf::Pose> lastOdomPose;
  if (m_motionModel->getLastOdomPose(lastOdomPose)){
    geometry_msgs::PoseStamped odomPoseMsg;
    tf::poseStampedTFToMsg(lastOdomPose, odomPoseMsg);
    m_poseOdomPub.publish(odomPoseMsg);
  }


  ////
  // Send tf odom->map
  ////
  tf::Stamped<tf::Pose> odomToMapTF;
  try{
    tf::Stamped<tf::Pose> baseToMapTF(bestParticlePose.inverse(),time, m_baseFrameId);
    m_tfListener.transformPose(m_odomFrameId, baseToMapTF, odomToMapTF);
  } catch (const tf::TransformException& e){
    ROS_WARN("Failed to subtract base to odom transform, will not publish pose estimate: %s", e.what());
    return;
  }

  tf::Transform latestTF(tf::Quaternion(odomToMapTF.getRotation()), tf::Point(odomToMapTF.getOrigin()));

  // We want to send a transform that is good up until a
  // tolerance time so that odom can be used
  // see ROS amcl_node

  ros::Duration transformTolerance(m_transformTolerance);
  ros::Time transformExpiration = (time + transformTolerance);

  tf::StampedTransform tmp_tf_stamped(latestTF.inverse(), transformExpiration, m_globalFrameId, m_odomFrameId);

  m_tfBroadcaster.sendTransform(tmp_tf_stamped);

}

unsigned HumanoidLocalization::getBestParticleIdx() const{
  if (m_bestParticleIdx < 0 || m_bestParticleIdx >= m_numParticles){
    ROS_WARN("Index (%d) of best particle not valid, using 0 instead", m_bestParticleIdx);
    return 0;
  }

  return m_bestParticleIdx;
}

tf::Pose HumanoidLocalization::getParticlePose(unsigned particleIdx) const{
  return m_particles.at(particleIdx).pose;
}

tf::Pose HumanoidLocalization::getBestParticlePose() const{
  return getParticlePose(getBestParticleIdx());
}

tf::Pose HumanoidLocalization::getMeanParticlePose() const{
  tf::Pose meanPose = tf::Pose::getIdentity();

  double totalWeight = 0.0;

  meanPose.setBasis(btMatrix3x3(0,0,0,0,0,0,0,0,0));
  for (Particles::const_iterator it = m_particles.begin(); it != m_particles.end(); ++it){
    meanPose.getOrigin() += it->pose.getOrigin() * it->weight;
    meanPose.getBasis()[0] += it->pose.getBasis()[0];
    meanPose.getBasis()[1] += it->pose.getBasis()[1];
    meanPose.getBasis()[2] += it->pose.getBasis()[2];
    totalWeight += it->weight;
  }
  assert(!isnan(totalWeight));

  //assert(totalWeight == 1.0);

  // just in case weights are not normalized:
  meanPose.getOrigin() /= totalWeight;
  // TODO: only rough estimate of mean rotation, asserts normalized weights!
  meanPose.getBasis() = meanPose.getBasis().scaled(btVector3(1.0/m_numParticles, 1.0/m_numParticles, 1.0/m_numParticles));

  // Apparently we need to normalize again
  meanPose.setRotation(meanPose.getRotation().normalized());

  return meanPose;
}

double HumanoidLocalization::nEff() const{

  double sqrWeights=0.0;
  for (Particles::const_iterator it=m_particles.begin(); it!=m_particles.end(); ++it){
    sqrWeights+=(it->weight * it->weight);
  }

  if (sqrWeights > 0.0)
    return 1./sqrWeights;
  else
    return 0.0;
}

void HumanoidLocalization::toLogForm(){
  // TODO: linear offset needed?
#pragma omp parallel for
  for (unsigned i = 0; i < m_particles.size(); ++i){
    assert(m_particles[i].weight > 0.0);
    m_particles[i].weight = log(m_particles[i].weight);
  }
}

void HumanoidLocalization::pauseLocalizationCallback(const std_msgs::BoolConstPtr& msg){
  if (msg->data){
    if (!m_paused){
      m_paused = true;
      ROS_INFO("Localization paused");
    } else{
      ROS_WARN("Received a msg to pause localizatzion, but is already paused.");
    }
  } else{
    if (m_paused){
      m_paused = false;
      ROS_INFO("Localization resumed");
      // force laser integration:
      m_receivedSensorData = false;
    } else {
      ROS_WARN("Received a msg to resume localization, is not paused.");
    }

  }

}

bool HumanoidLocalization::pauseLocalizationSrvCallback(std_srvs::Empty::Request& req, std_srvs::Empty::Response& res)
{
  if (!m_paused){
    m_paused = true;
    ROS_INFO("Localization paused");
  } else{
    ROS_WARN("Received a request to pause localizatzion, but is already paused.");
  }

  return true;
}

bool HumanoidLocalization::resumeLocalizationSrvCallback(std_srvs::Empty::Request& req, std_srvs::Empty::Response& res)
{
  if (m_paused){
    m_paused = false;
    ROS_INFO("Localization resumed");
    // force next laser integration:
    m_receivedSensorData = false;
  } else {
    ROS_WARN("Received a request to resume localization, but is not paused.");
  }

  return true;
}

bool HumanoidLocalization::lookupPoseHeight(const ros::Time& t, double& poseHeight) const{
  tf::StampedTransform tf;
  if (m_motionModel->lookupLocalTransform(m_baseFootprintId, t, tf)){
    poseHeight = tf.getOrigin().getZ();
    return true;
  } else
    return false;
}

}


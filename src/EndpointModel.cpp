// SVN $HeadURL: http://alufr-ros-pkg.googlecode.com/svn/trunk/humanoid_stacks/humanoid_navigation/humanoid_localization/src/EndpointModel.cpp $
// SVN $Id: EndpointModel.cpp 3426 2012-10-17 13:33:04Z hornunga@informatik.uni-freiburg.de $

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

#include <humanoid_localization/EndpointModel.h>

namespace humanoid_localization{

EndpointModel::EndpointModel(ros::NodeHandle* nh, boost::shared_ptr<MapModel> mapModel, EngineT * rngEngine)
: ObservationModel(nh, mapModel, rngEngine), m_sigma(0.2), m_maxObstacleDistance(0.5)
{
  ROS_INFO("Using Endpoint observation model");

  nh->param("endpoint/sigma", m_sigma, m_sigma);
  nh->param("endpoint/max_obstacle_distance", m_maxObstacleDistance, m_maxObstacleDistance);

  if (m_sigma <= 0.0){
    ROS_ERROR("Sigma (std.dev) needs to be > 0 in EndpointModel");
  }

  initDistanceMap();


}

EndpointModel::~EndpointModel(){

}

void EndpointModel::integrateMeasurement(Particles& particles, const PointCloud& pc, const std::vector<float>& ranges, float max_range, const tf::Transform& baseToSensor){

    // iterate over samples, multithreaded:
#pragma omp parallel for
  for (unsigned i=0; i < particles.size(); ++i){
    Eigen::Matrix4f globalLaserOrigin;
    pcl_ros::transformAsMatrix(particles[i].pose * baseToSensor, globalLaserOrigin);
    PointCloud pc_transformed;
    pcl::transformPointCloud(pc, pc_transformed, globalLaserOrigin);

    // iterate over beams:
    for (PointCloud::const_iterator it = pc_transformed.begin(); it != pc_transformed.end(); ++it){
      // search only for endpoint in tree
      octomap::point3d endPoint(it->x, it->y, it->z);
      float dist = m_distanceMap->getDistance(endPoint);
      if (dist > 0.0){ // endpoint is inside map:
        //std::cout << dist << " ";
        particles[i].weight += logLikelihood(dist, m_sigma);
      } else { //assign weight of max.distance:
        particles[i].weight += logLikelihood(m_maxObstacleDistance, m_sigma);
      }
    }
    // TODO: handle max range measurements
    //std::cout << "\n";
  }

}

bool EndpointModel::getHeightError(const Particle& p, const tf::StampedTransform& footprintToBase, double& heightError) const{
  tf::Vector3 xyz = p.pose.getOrigin();
  double poseHeight = xyz.getZ();
  std::vector<double> heights;
  m_mapModel->getHeightlist(xyz.getX(), xyz.getY(), 0.6, heights);
  if (heights.size() == 0)
    return false;


  // find nearest z-level:
  heightError = std::numeric_limits<double>::max();
  for (unsigned i = 0; i< heights.size(); i++){
    double dist = std::abs((heights[i] + poseHeight) - xyz.getZ());
    if (dist < heightError)
      heightError = dist;

  }

  return true;
}

void EndpointModel::setMap(boost::shared_ptr<octomap::OcTree> map){
  m_map = map;
  initDistanceMap();
}

void EndpointModel::initDistanceMap(){
  double x,y,z;
  m_map->getMetricMin(x,y,z);
  octomap::point3d min(x,y,z);
  m_map->getMetricMax(x,y,z);
  octomap::point3d max(x,y,z);
  m_distanceMap = boost::shared_ptr<DynamicEDTOctomap>(new DynamicEDTOctomap(float(m_maxObstacleDistance), &(*m_map), min, max, false));
  m_distanceMap->update();
  ROS_INFO("Distance map for endpoint model completed");
}

}


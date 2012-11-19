// SVN $HeadURL: http://alufr-ros-pkg.googlecode.com/svn/trunk/humanoid_stacks/humanoid_navigation/humanoid_localization/src/RaycastingModel.cpp $
// SVN $Id: RaycastingModel.cpp 3426 2012-10-17 13:33:04Z hornunga@informatik.uni-freiburg.de $

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

#include <humanoid_localization/RaycastingModel.h>

#include <pcl/point_types.h>
#include <pcl/ros/conversions.h>
#include <pcl_ros/transforms.h>
#include <octomap_ros/conversions.h>

namespace humanoid_localization{

RaycastingModel::RaycastingModel(ros::NodeHandle* nh, boost::shared_ptr<MapModel> mapModel, EngineT * rngEngine)
: ObservationModel(nh, mapModel, rngEngine)
{
  // params:
  nh->param("raycasting/z_hit", m_zHit, 0.8);
  nh->param("raycasting/z_short", m_zShort, 0.1);
  nh->param("raycasting/z_max", m_zMax, 0.05);
  nh->param("raycasting/z_rand", m_zRand, 0.05);
  nh->param("raycasting/sigma_hit", m_sigmaHit, 0.05);
  nh->param("raycasting/lambda_short", m_lambdaShort, 0.1);

  // TODO: move to main localization code for point cloud handling
//  nh->param("filter_ground", m_filterPointCloudGround, true);
//  // distance of points from plane for RANSAC
//  nh->param("ground_filter/distance", m_groundFilterDistance, 0.04);
//  // angular derivation of found plane:
//  nh->param("ground_filter/angle", m_groundFilterAngle, 0.15);
//  // distance of found plane from z=0 to be detected as ground (e.g. to exclude tables)
//  nh->param("ground_filter/plane_distance", m_groundFilterPlaneDistance, 0.07);
//  nh->param("ground_filter/num_ground_points", m_numFloorPoints, 20);
//  nh->param("ground_filter/num_non_ground_points", m_numNonFloorPoints, 80);
}

RaycastingModel::~RaycastingModel(){

}

void RaycastingModel::integrateMeasurement(Particles& particles, const PointCloud& pc, const std::vector<float>& ranges, float max_range, const tf::Transform& base_to_laser){

  if (!m_map){
    ROS_ERROR("Map file is not set in raycasting");
    return;
  }
  // iterate over samples, multi-threaded:
#pragma omp parallel for
  for (unsigned i=0; i < particles.size(); ++i){
    Eigen::Matrix4f globalLaserOrigin;
    tf::Transform globalLaserOriginTf = particles[i].pose * base_to_laser;
    pcl_ros::transformAsMatrix(globalLaserOriginTf, globalLaserOrigin);

    // raycasting origin
    octomap::point3d originP(globalLaserOriginTf.getOrigin().x(),
                             globalLaserOriginTf.getOrigin().y(),
                             globalLaserOriginTf.getOrigin().z());
    PointCloud pc_transformed;
    pcl::transformPointCloud(pc, pc_transformed, globalLaserOrigin);

    // iterate over beams:
    PointCloud::const_iterator pc_it = pc_transformed.begin();
    std::vector<float>::const_iterator ranges_it = ranges.begin();
    for ( ; pc_it != pc_transformed.end(); ++pc_it, ++ranges_it){

      double p = 0.0; // probability for weight

      if (*ranges_it <= max_range){

        // direction of ray in global (map) coords
        octomap::point3d direction(pc_it->x , pc_it->y, pc_it->z);
        direction = direction - originP;

        octomap::point3d end;
        if(m_map->castRay(originP,direction, end, true, max_range)){
          // TODO: use squared distances?
          float raycastRange = (originP - end).norm();
          float z = raycastRange - *ranges_it;

          // obstacle hit:
          p = m_zHit / (SQRT_2_PI * m_sigmaHit) * exp(-(z * z) / (2 * m_sigmaHit * m_sigmaHit));

          // short range:
          if (*ranges_it <= raycastRange)
            p += m_zShort * m_lambdaShort * exp(-m_lambdaShort* (*ranges_it)) / (1-exp(-m_lambdaShort*raycastRange));

          // random measurement:
          p += m_zRand / max_range;
        } else { // racasting did not hit, but measurement is no maxrange => random?
          p = m_zRand / max_range;
        }

      } else{ // maximum range
        p = m_zMax;

      }

      // add log-likelihood
      // (note: likelihood can be larger than 1!)
      assert(p > 0.0);
      particles[i].weight += log(p);

    } // end of loop over scan

  } // end of loop over particles


}

bool RaycastingModel::getHeightError(const Particle& p, const tf::StampedTransform& footprintToBase, double& heightError) const{

  octomap::point3d direction = octomap::pointTfToOctomap(footprintToBase.inverse().getOrigin());
  octomap::point3d origin = octomap::pointTfToOctomap(p.pose.getOrigin());
  octomap::point3d end;
  // cast ray to bottom:
  if (!m_map->castRay(origin, direction, end, true, 2*direction.norm()))
    return false;

  heightError =  std::max(0.0, std::abs((origin-end).z() - p.pose.getOrigin().getZ()) - m_map->getResolution());
//  ROS_INFO("Height error: %f", heightError);

  return true;
}

}

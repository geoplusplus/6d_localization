// SVN $HeadURL: http://alufr-ros-pkg.googlecode.com/svn/trunk/humanoid_stacks/humanoid_navigation/humanoid_localization/include/humanoid_localization/MapModel.h $
// SVN $Id: MapModel.h 3384 2012-10-04 14:33:06Z hornunga@informatik.uni-freiburg.de $

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

#ifndef HUMANOID_LOCALIZATION_MAPMODEL_H_
#define HUMANOID_LOCALIZATION_MAPMODEL_H_

#include <omp.h>
#include <ros/ros.h>
#include <octomap_msgs/conversions.h>
#include <octomap_msgs/GetOctomap.h>

#include <humanoid_localization/humanoid_localization_defs.h>

namespace humanoid_localization{

class MapModel{
public:
  MapModel(ros::NodeHandle* nh);
  virtual ~MapModel();

  //void setMap(boost::shared_ptr<octomap::OcTree> map);
  boost::shared_ptr<octomap::OcTree> getMap() const;

  /**
   * Check if particles represent valid poses:
   * Must be within map bounding box and not in an occupied area.
   * Otherwise weight is minimized (=> die out at next resampling)
   */
  virtual void verifyPoses(Particles& particles);

  virtual void initGlobal(Particles& particles,
                          const Vector6d& initPose, const Vector6d& initNoise,
                          UniformGeneratorT& rngUniform, NormalGeneratorT& rngNormal);

  /// @return whether a map coordinate is occupied. Will return
  /// "false" if the coordinate does not exist in the map (e.g. out of bounds).
  virtual bool isOccupied(const octomap::point3d& position) const;
  virtual bool isOccupied(octomap::OcTreeNode* node) const = 0;

  /**
   * Get a list of valid z values at a given xy-position
   *
   * @param x
   * @param y
   * @param totalHeight clearance of the robot required to be free
   * @param[out] heights list of valid heights, return by ref.
   */
  void getHeightlist(double x, double y, double totalHeight, std::vector<double>& heights);


protected:
  boost::shared_ptr<octomap::OcTree> m_map;

  double m_motionMeanZ;
  double m_motionRangeZ;
  double m_motionRangeRoll;
  double m_motionRangePitch;
  double m_motionObstacleDist;

};


class DistanceMap : public MapModel{
public:
  DistanceMap(ros::NodeHandle* nh);
  virtual ~DistanceMap();

  virtual bool isOccupied(octomap::OcTreeNode* node) const;

};


class OccupancyMap : public MapModel{
public:
  OccupancyMap(ros::NodeHandle* nh);
  virtual ~OccupancyMap();

  virtual bool isOccupied(octomap::OcTreeNode* node) const;
};

}
#endif

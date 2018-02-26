// This file is part of the AliceVision project.
// Copyright (c) 2016 AliceVision contributors.
// Copyright (c) 2012 openMVG contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "sfmDataIO_baf.hpp"

#include <dependencies/stlplus3/filesystemSimplified/file_system.hpp>

#include <fstream>

namespace aliceVision {
namespace sfm {

bool saveBAF(
  const SfMData& sfmData,
  const std::string& filename,
  ESfMData partFlag)
{
  std::ofstream stream(filename.c_str());
  if (!stream.is_open())
    return false;

  bool bOk = false;
  {
    stream
      << sfmData.GetIntrinsics().size() << '\n'
      << sfmData.GetViews().size() << '\n'
      << sfmData.GetLandmarks().size() << '\n';

    const Intrinsics & intrinsics = sfmData.GetIntrinsics();
    for (Intrinsics::const_iterator iterIntrinsic = intrinsics.begin();
      iterIntrinsic != intrinsics.end(); ++iterIntrinsic)
    {
      //get params
      const std::vector<double> intrinsicsParams = iterIntrinsic->second.get()->getParams();
      std::copy(intrinsicsParams.begin(), intrinsicsParams.end(),
        std::ostream_iterator<double>(stream, " "));
      stream << '\n';
    }

    const Poses & poses = sfmData.GetPoses();
    for (Views::const_iterator iterV = sfmData.GetViews().begin();
      iterV != sfmData.GetViews().end();
      ++ iterV)
    {
      const View * view = iterV->second.get();
      if (!sfmData.IsPoseAndIntrinsicDefined(view))
      {
        const Mat3 R = Mat3::Identity();
        const double * rotation = R.data();
        std::copy(rotation, rotation+9, std::ostream_iterator<double>(stream, " "));
        const Vec3 C = Vec3::Zero();
        const double * center = C.data();
        std::copy(center, center+3, std::ostream_iterator<double>(stream, " "));
        stream << '\n';
      }
      else
      {
        // [Rotation col major 3x3; camera center 3x1]
        const double * rotation = poses.at(view->getPoseId()).rotation().data();
        std::copy(rotation, rotation+9, std::ostream_iterator<double>(stream, " "));
        const double * center = poses.at(view->getPoseId()).center().data();
        std::copy(center, center+3, std::ostream_iterator<double>(stream, " "));
        stream << '\n';
      }
    }

    const Landmarks & landmarks = sfmData.GetLandmarks();
    for (Landmarks::const_iterator iterLandmarks = landmarks.begin();
      iterLandmarks != landmarks.end();
      ++iterLandmarks)
    {
      // Export visibility information
      // X Y Z #observations id_cam id_pose x y ...
      const double * X = iterLandmarks->second.X.data();
      std::copy(X, X+3, std::ostream_iterator<double>(stream, " "));
      const Observations & observations = iterLandmarks->second.observations;
      stream << observations.size() << " ";
      for (Observations::const_iterator iterOb = observations.begin();
        iterOb != observations.end(); ++iterOb)
      {
        const IndexT id_view = iterOb->first;
        const View * v = sfmData.GetViews().at(id_view).get();
        stream
          << v->getIntrinsicId() << ' '
          << v->getPoseId() << ' '
          << iterOb->second.x(0) << ' ' << iterOb->second.x(1) << ' ';
      }
      stream << '\n';
    }

    stream.flush();
    bOk = stream.good();
    stream.close();
  }

  // Export View filenames & ids as an imgList.txt file
  {
    const std::string sFile = stlplus::create_filespec(
      stlplus::folder_part(filename), stlplus::basename_part(filename) + std::string("_imgList"), "txt");

    stream.open(sFile.c_str());
    if (!stream.is_open())
      return false;
    for (Views::const_iterator iterV = sfmData.GetViews().begin();
      iterV != sfmData.GetViews().end();
      ++ iterV)
    {
      const std::string sView_filename = iterV->second->getImagePath();
      stream
        << sView_filename
        << ' ' << iterV->second->getIntrinsicId()
        << ' ' << iterV->second->getPoseId() << "\n";
    }
    stream.flush();
    bOk = stream.good();
    stream.close();
  }
  return bOk;
}

} // namespace sfm
} // namespace aliceVision

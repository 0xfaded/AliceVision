// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "viewIO.hpp"

#include <aliceVision/numeric/numeric.hpp>
#include <aliceVision/sfm/utils/uid.hpp>
#include <aliceVision/camera/camera.hpp>
#include <aliceVision/image/io.hpp>

#include <boost/filesystem.hpp>

#include <stdexcept>

namespace fs = boost::filesystem;

namespace aliceVision {
namespace sfm {

void updateIncompleteView(View& view)
{
  // check if the view is complete
  if(view.getViewId() != UndefinedIndexT &&
     view.getIntrinsicId() != UndefinedIndexT &&
     view.getPoseId() == view.getViewId() &&
     view.getHeight() > 0 &&
     view.getWidth() >  0)
    return;

  int width, height;
  std::map<std::string, std::string> metadata;

  image::readImageMetadata(view.getImagePath(), width, height, metadata);

  view.setWidth(width);
  view.setHeight(height);

  // reset metadata
  if(view.getMetadata().empty())
    view.setMetadata(metadata);

  // reset viewId
  view.setViewId(computeUID(view));

  if(view.getPoseId() == UndefinedIndexT)
  {
    // check if the rig poseId id is defined
    if(view.isPartOfRig())
    {
      ALICEVISION_LOG_ERROR("Error: Can't find poseId for'" << fs::path(view.getImagePath()).filename().string() << "' marked as part of a rig." << std::endl);
      throw std::invalid_argument("Error: Can't find poseId for'" + fs::path(view.getImagePath()).filename().string() + "' marked as part of a rig.");
    }
    else
      view.setPoseId(view.getViewId());
  }
  else if((!view.isPartOfRig()) && (view.getPoseId() != view.getViewId()))
  {
    ALICEVISION_LOG_ERROR("Error: Bad poseId for image '" << fs::path(view.getImagePath()).filename().string() << "' (viewId should be equal to poseId)." << std::endl);
    throw std::invalid_argument("Error: Bad poseId for image '" + fs::path(view.getImagePath()).filename().string() + "'.");
  }
}

std::shared_ptr<camera::IntrinsicBase> getViewIntrinsic(const View& view,
                                                        double sensorWidth,
                                                        double defaultFocalLengthPx,
                                                        double defaultFieldOfView,
                                                        camera::EINTRINSIC defaultIntrinsicType,
                                                        double defaultPPx,
                                                        double defaultPPy)
{
  // can't combine defaultFocalLengthPx and defaultFieldOfView
  assert(defaultFocalLengthPx < 0 || defaultFieldOfView < 0);

  // get view informations
  const std::string cameraBrand = view.getMetadataOrEmpty("Make");
  const std::string cameraModel = view.getMetadataOrEmpty("Model");
  const std::string bodySerialNumber = view.getMetadataOrEmpty("Exif:BodySerialNumber");
  const std::string lensSerialNumber = view.getMetadataOrEmpty("Exif:LensSerialNumber");

  float mmFocalLength = view.hasMetadata("Exif:FocalLength") ? std::stof(view.getMetadata("Exif:FocalLength")) : -1;
  double pxFocalLength;

  if(defaultFocalLengthPx > 0)
  {
    pxFocalLength = defaultFocalLengthPx;
  }

  if(defaultFieldOfView > 0)
  {
    const double focalRatio = 0.5 / std::tan(0.5 * degreeToRadian(defaultFieldOfView));
    pxFocalLength = focalRatio * std::max(view.getWidth(), view.getHeight());
  }

  camera::EINTRINSIC intrinsicType = defaultIntrinsicType;

  double ppx = view.getWidth() / 2.0;
  double ppy = view.getHeight() / 2.0;

  bool isResized = false;

  if(view.hasMetadata("Exif:PixelXDimension") && view.hasMetadata("Exif:PixelYDimension")) // has dimension metadata
  {
    // check if the image is resized
    int exifWidth = std::stoi(view.getMetadata("Exif:PixelXDimension"));
    int exifHeight = std::stoi(view.getMetadata("Exif:PixelYDimension"));

    // if metadata is rotated
    if(exifWidth == view.getHeight() && exifHeight == view.getWidth())
      std::swap(exifWidth, exifHeight);


    if(exifWidth > 0 && exifHeight > 0 &&
       (exifWidth != view.getWidth() || exifHeight != view.getHeight()))
    {
      ALICEVISION_LOG_WARNING("Warning: Resized image detected: " << fs::path(view.getImagePath()).filename().string() << std::endl
                          << "\t- real image size: " <<  view.getWidth() << "x" <<  view.getHeight() << std::endl
                          << "\t- image size from exif metadata is: " << exifWidth << "x" << exifHeight << std::endl);
      isResized = true;
    }
  }
  else if(defaultPPx > 0 && defaultPPy > 0) // use default principal point
  {
    ppx = defaultPPx;
    ppy = defaultPPy;
  }

  // handle case where focal length (mm) is unset or false
  if(mmFocalLength <= .0f)
  {
    ALICEVISION_LOG_WARNING("Warning: image '" << fs::path(view.getImagePath()).filename().string() << "' focal length (in mm) metadata is missing." << std::endl
                                               << "Can't compute focal length (px), use default." << std::endl);
  }
  else if(sensorWidth > 0)
  {
    // Retrieve the focal from the metadata in mm and convert to pixel.
    pxFocalLength = std::max(view.getWidth(), view.getHeight()) * mmFocalLength / sensorWidth;
  }

  // choose intrinsic type
  if(cameraBrand == "Custom")
  {
    intrinsicType = camera::EINTRINSIC_stringToEnum(cameraModel);
  }
  else if(isResized)
  {
    // if the image has been resized, we assume that it has been undistorted
    // and we use a camera without lens distortion.
    intrinsicType = camera::PINHOLE_CAMERA;
  }
  else if((mmFocalLength > 0.0 && mmFocalLength < 15) || (defaultFieldOfView > 95))
  {
    // if the focal lens is short, the fisheye model should fit better.
    intrinsicType = camera::PINHOLE_CAMERA_FISHEYE;
  }
  else if(intrinsicType == camera::PINHOLE_CAMERA_START)
  {
    // choose a default camera model if no default type
    // use standard lens with radial distortion by default
    intrinsicType = camera::PINHOLE_CAMERA_RADIAL3;
  }

  // create the desired intrinsic
  std::shared_ptr<camera::IntrinsicBase> intrinsic = camera::createPinholeIntrinsic(intrinsicType, view.getWidth(), view.getHeight(), pxFocalLength, ppx, ppy);
  intrinsic->setInitialFocalLengthPix(pxFocalLength);

  // initialize distortion parameters
  switch(intrinsicType)
  {
    case camera::PINHOLE_CAMERA_FISHEYE:
    {
      if(cameraBrand == "GoPro")
        intrinsic->updateFromParams({pxFocalLength, ppx, ppy, 0.0524, 0.0094, -0.0037, -0.0004});
      break;
    }
    case camera::PINHOLE_CAMERA_FISHEYE1:
    {
      if(cameraBrand == "GoPro")
        intrinsic->updateFromParams({pxFocalLength, ppx, ppy, 1.04});
      break;
    }
    default: break;
  }

  // create serial number
  intrinsic->setSerialNumber(bodySerialNumber + lensSerialNumber);

  return intrinsic;
}

} // namespace sfm
} // namespace aliceVision

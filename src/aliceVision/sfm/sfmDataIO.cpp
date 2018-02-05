// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "sfmDataIO.hpp"
#include <aliceVision/config.hpp>
#include <aliceVision/stl/mapUtils.hpp>
#include <aliceVision/sfm/sfmDataIO_json.hpp>
#include <aliceVision/sfm/sfmDataIO_ply.hpp>
#include <aliceVision/sfm/sfmDataIO_baf.hpp>
#include <aliceVision/sfm/sfmDataIO_gt.hpp>

#if ALICEVISION_IS_DEFINED(ALICEVISION_HAVE_ALEMBIC)
#include <aliceVision/sfm/AlembicExporter.hpp>
#include <aliceVision/sfm/AlembicImporter.hpp>
#endif

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace aliceVision {
namespace sfm {

///Check that each pose have a valid intrinsic and pose id in the existing View ids
bool ValidIds(const SfMData& sfmData, ESfMData partFlag)
{
  const bool bCheck_Intrinsic = (partFlag & INTRINSICS);
  const bool bCheck_Extrinsic = (partFlag & EXTRINSICS);

  std::set<IndexT> intrinsicIdsDeclared;
  transform(sfmData.GetIntrinsics().begin(), sfmData.GetIntrinsics().end(),
    std::inserter(intrinsicIdsDeclared, intrinsicIdsDeclared.begin()), stl::RetrieveKey());

  std::set<IndexT> extrinsicIdsDeclared; //unique so can use a set
  transform(sfmData.GetPoses().begin(), sfmData.GetPoses().end(),
    std::inserter(extrinsicIdsDeclared, extrinsicIdsDeclared.begin()), stl::RetrieveKey());

  // Collect id_intrinsic and id_extrinsic referenced from views
  std::set<IndexT> intrinsicIdsReferenced;
  std::set<IndexT> extrinsicIdsReferenced;
  for(const auto& v: sfmData.GetViews())
  {
    const IndexT id_intrinsic = v.second.get()->getIntrinsicId();
    intrinsicIdsReferenced.insert(id_intrinsic);

    const IndexT id_pose = v.second.get()->getPoseId();
    extrinsicIdsReferenced.insert(id_pose);
  }

  // We may have some views with undefined Intrinsics,
  // so erase the UndefinedIndex value if exist.
  intrinsicIdsReferenced.erase(UndefinedIndexT);
  extrinsicIdsReferenced.erase(UndefinedIndexT);

  // Check if defined intrinsic & extrinsic are at least connected to views
  bool bRet = true;
  if(bCheck_Intrinsic && intrinsicIdsDeclared != intrinsicIdsReferenced)
  {
    ALICEVISION_LOG_WARNING("The number of intrinsics is incoherent:");
    ALICEVISION_LOG_WARNING(intrinsicIdsDeclared.size() << " intrinsics declared and " << intrinsicIdsReferenced.size() << " intrinsics used.");
    std::set<IndexT> undefinedIntrinsicIds;
    // undefinedIntrinsicIds = intrinsicIdsReferenced - intrinsicIdsDeclared
    std::set_difference(intrinsicIdsReferenced.begin(), intrinsicIdsReferenced.end(),
                        intrinsicIdsDeclared.begin(), intrinsicIdsDeclared.end(), 
                        std::inserter(undefinedIntrinsicIds, undefinedIntrinsicIds.begin()));
    // If undefinedIntrinsicIds is not empty,
    // some intrinsics are used in Views but never declared.
    // So the file structure is invalid and may create troubles.
    if(!undefinedIntrinsicIds.empty())
      bRet = false; // error
  }
  
  if (bCheck_Extrinsic && extrinsicIdsDeclared != extrinsicIdsReferenced)
  {
    ALICEVISION_LOG_TRACE(extrinsicIdsDeclared.size() << " extrinsics declared and " << extrinsicIdsReferenced.size() << " extrinsics used.");
    std::set<IndexT> undefinedExtrinsicIds;
    // undefinedExtrinsicIds = extrinsicIdsReferenced - extrinsicIdsDeclared
    std::set_difference(extrinsicIdsDeclared.begin(), extrinsicIdsDeclared.end(),
                        extrinsicIdsReferenced.begin(), extrinsicIdsReferenced.end(),
                        std::inserter(undefinedExtrinsicIds, undefinedExtrinsicIds.begin()));
    // If undefinedExtrinsicIds is not empty,
    // some extrinsics are used in Views but never declared.
    // So the file structure is invalid and may create troubles.
    if(!undefinedExtrinsicIds.empty())
      bRet = false; // error
  }

  return bRet;
}

bool Load(SfMData& sfmData, const std::string& filename, ESfMData partFlag)
{
  const std::string ext = fs::extension(filename);
  bool status = false;

  if(ext == ".sfm" || ext == ".json") // JSON File
  {
    status = loadJSON(sfmData, filename, partFlag);
  }
#if ALICEVISION_IS_DEFINED(ALICEVISION_HAVE_ALEMBIC)
  else if(ext == ".abc") // Alembic
  {
    AlembicImporter(filename).populateSfM(sfmData, partFlag);
    status = true;
  }
#endif // ALICEVISION_HAVE_ALEMBIC
  else if(fs::is_directory(filename))
  {
    status = readGt(filename, sfmData);
  }
  else // It is not a folder or known format, return false
  {
    ALICEVISION_LOG_ERROR("Unknown input SfM data format: '" << ext << "'");
    return false;
  }

  // Assert that loaded intrinsics | extrinsics are linked to valid view
  if(status && (partFlag & VIEWS) && ((partFlag & INTRINSICS) || (partFlag & EXTRINSICS)))
    return ValidIds(sfmData, partFlag);

  return status;
}

bool Save(const SfMData& sfmData, const std::string& filename, ESfMData partFlag)
{
  const std::string ext = fs::extension(filename);

  if(ext == ".sfm" || ext == ".json") // JSON File
  {
    return saveJSON(sfmData, filename, partFlag);
  }
  else if(ext == ".ply") // Polygon File
  {
    return savePLY(sfmData, filename, partFlag);
  }
  else if (ext == ".baf") // Bundle Adjustment File
  {
    return saveBAF(sfmData, filename, partFlag);
  }
#if ALICEVISION_IS_DEFINED(ALICEVISION_HAVE_ALEMBIC)
  else if (ext == ".abc") // Alembic
  {
    aliceVision::sfm::AlembicExporter(filename).addSfM(sfmData, partFlag);
    return true;
  }
#endif // ALICEVISION_HAVE_ALEMBIC
  ALICEVISION_LOG_ERROR("Cannot save the SfM data file: '" << filename << "'."
                        << std::endl << "The file extension is not recognized.");
  return false;
}

} // namespace sfm
} // namespace aliceVision



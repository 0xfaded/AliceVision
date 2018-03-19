// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include <aliceVision/sfm/sfm.hpp>
#include <aliceVision/system/Timer.hpp>
#include <aliceVision/matchingImageCollection/pairBuilder.hpp>
#include <aliceVision/system/Logger.hpp>
#include <aliceVision/system/cmdline.hpp>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <cstdlib>

using namespace aliceVision;
using namespace aliceVision::sfm;
using namespace std;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

/// Build a list of pair that share visibility content from the SfMData structure
PairSet BuildPairsFromStructureObservations(const SfMData& sfmData)
{
  PairSet pairs;

  for (Landmarks::const_iterator itL = sfmData.GetLandmarks().begin();
    itL != sfmData.GetLandmarks().end(); ++itL)
  {
    const Landmark & landmark = itL->second;
    for(const auto& iterI : landmark.observations)
    {
      const IndexT id_viewI = iterI.first;
      Observations::const_iterator iterJ = landmark.observations.begin();
      std::advance(iterJ, 1);
      for (; iterJ != landmark.observations.end(); ++iterJ)
      {
        const IndexT id_viewJ = iterJ->first;
        pairs.insert( std::make_pair(id_viewI,id_viewJ));
      }
    }
  }
  return pairs;
}

/// Build a list of pair from the camera frusta intersections
PairSet BuildPairsFromFrustumsIntersections(
  const SfMData & sfmData,
  const double z_near = -1., // default near plane
  const double z_far = -1.,  // default far plane
  const std::string& sOutDirectory = "") // output folder to save frustums as PLY
{
  const FrustumFilter frustum_filter(sfmData, z_near, z_far);
  if (!sOutDirectory.empty())
    frustum_filter.export_Ply((fs::path(sOutDirectory) / "frustums.ply").string());
  return frustum_filter.getFrustumIntersectionPairs();
}

int main(int argc, char **argv)
{
  // command-line parameters

  std::string verboseLevel = system::EVerboseLevel_enumToString(system::Logger::getDefaultVerboseLevel());
  std::string sfmDataFilename;
  std::string outputFilename;
  double zNear = -1.;
  double zFar = -1.;

  po::options_description allParams(
    "Compute camera cones that share some putative visual content.\n"
    "AliceVision FrustumFiltering");

  po::options_description requiredParams("Required parameters");
  requiredParams.add_options()
    ("input,i", po::value<std::string>(&sfmDataFilename)->required(),
      "SfMData file.")
    ("output,o", po::value<std::string>(&outputFilename)->required(),
      "Output pair filename.");

  po::options_description optionalParams("Optional parameters");
  optionalParams.add_options()
    ("zNear", po::value<double>(&zNear)->default_value(zNear),
      "Distance of the near camera plane.")
    ("zFar", po::value<double>(&zFar)->default_value(zFar),
      "Distance of the far camera plane.");

  po::options_description logParams("Log parameters");
  logParams.add_options()
    ("verboseLevel,v", po::value<std::string>(&verboseLevel)->default_value(verboseLevel),
      "verbosity level (fatal,  error, warning, info, debug, trace).");

  allParams.add(requiredParams).add(optionalParams).add(logParams);

  po::variables_map vm;
  try
  {
    po::store(po::parse_command_line(argc, argv, allParams), vm);

    if(vm.count("help") || (argc == 1))
    {
      ALICEVISION_COUT(allParams);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch(boost::program_options::required_option& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }
  catch(boost::program_options::error& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }

  ALICEVISION_COUT("Program called with the following parameters:");
  ALICEVISION_COUT(vm);

  // set verbose level
  system::Logger::get()->setLogLevel(verboseLevel);

  // check that we can create the output folder
  if(!fs::exists(fs::path(outputFilename).parent_path()))
    if(!fs::exists(fs::path(outputFilename).parent_path()))
      return EXIT_FAILURE;

  // load input SfMData scene
  SfMData sfmData;
  if(!Load(sfmData, sfmDataFilename, ESfMData::ALL))
  {
    ALICEVISION_LOG_ERROR("The input SfMData file '"<< sfmDataFilename << "' cannot be read");
    return EXIT_FAILURE;
  }

  aliceVision::system::Timer timer;

  const PairSet pairs = BuildPairsFromFrustumsIntersections(sfmData, zNear, zFar, fs::path(outputFilename).parent_path().string());
  /*const PairSet pairs = BuildPairsFromStructureObservations(sfm_data); */

  ALICEVISION_LOG_INFO("# pairs: " << pairs.size());
  ALICEVISION_LOG_INFO("Pair filtering took: " << timer.elapsed() << " s");

  // export pairs on disk
  if(savePairs(outputFilename, pairs))
    return EXIT_SUCCESS;
  else
    return EXIT_FAILURE;
}

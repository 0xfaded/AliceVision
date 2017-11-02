// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include "aliceVision/config.hpp"
#include "aliceVision/system/Logger.hpp"
#include "aliceVision/exif/sensorWidthDatabase/parseDatabase.hpp"

#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char ** argv)
{
  std::string sensorDatabasePath;
  std::string brandName;
  std::string modelName;

  po::options_description allParams("AliceVision Sample parseDatabase");
  allParams.add_options()
    ("sensorDatabase,s", po::value<std::string>(&sensorDatabasePath)->required(),
      "Camera sensor width database path.")
    ("brand,b", po::value<std::string>(&brandName)->required(),
      "Camera brand.")
    ("model,m", po::value<std::string>(&modelName)->required(),
      "Camera model.");

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

  std::vector<aliceVision::exif::sensordb::Datasheet> vec_database;
  aliceVision::exif::sensordb::Datasheet datasheet;

  if ( !aliceVision::exif::sensordb::parseDatabase( sensorDatabasePath, vec_database ) )
  {
    std::cout << "Database creation failure from the file : " << sensorDatabasePath  << std::endl;
    return EXIT_FAILURE;
  }

  if ( !aliceVision::exif::sensordb::getInfo( brandName, modelName, vec_database, datasheet ) )
  {
    std::cout << "The camera " << modelName << " doesn't exist in the database" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Result : " << std::endl << datasheet._brand << "\t" << datasheet._model << "\t" << datasheet._sensorSize << std::endl;

  return EXIT_SUCCESS;
}


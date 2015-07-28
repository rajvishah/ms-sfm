#include "../base/defs.h"
#include "../base/MatchPairs.h"
#include "../base/argvparser.h"
using namespace CommandLineProcessing;

void SetupCommandlineParser(ArgvParser& cmd, int argc, char* argv[]) {
  cmd.setIntroductoryDescription("Match pairs of images guided by geometry from Bundle File ");

  //define error codes
  cmd.addErrorCode(0, "Success");
  cmd.addErrorCode(1, "Error");

  cmd.setHelpOption("h", "help",""); 
  cmd.defineOption("base_dir", "Path to base directory that stores all list files\n--list_keys.txt\n--list_images.txt\n--image_dims.txt\nnum_sifts.txt", 
      ArgvParser::OptionRequired);
  cmd.defineOption("bundle_dir", "Path to the bundle file dir", ArgvParser::OptionRequired);

  cmd.defineOption("result_dir", "Path to base directory that stores all matches files", 
      ArgvParser::OptionRequired);
  
  cmd.defineOption("list_file", "Path to the file of candidate pairs to match", 
      ArgvParser::OptionRequired);
  
  /// finally parse and handle return codes (display help etc...)
  int result = cmd.parse(argc, argv);
  if (result != ArgvParser::NoParserError) {
    cout << cmd.parseErrorDescription(result);
    exit(-1);
  }
}

int main(int argc, char* argv[]) {
    ArgvParser cmd;
    SetupCommandlineParser(cmd, argc, argv);

    string bundleDir = cmd.optionValue("bundle_dir");
    string baseDir = cmd.optionValue("base_dir");
    string resultDir = cmd.optionValue("result_dir");
    string listFilePath = cmd.optionValue("list_file");

    sfm::MatchPairs mg(bundleDir,baseDir, -1);
    if(!mg.isInitialized()) {
        printf("\nNot Initialized Properly");
        fflush(stdout);
        return -1;
    }

    bool status = mg.matchListPairs( listFilePath, resultDir );
    if(!status) {
        printf("\nError: failed to match pairs in list %s", listFilePath.c_str());
    }
    return 0;
}

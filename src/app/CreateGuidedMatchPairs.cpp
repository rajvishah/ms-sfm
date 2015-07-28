#include "../base/defs.h"
#include "../base/MatchPairs.h"
#include "../base/argvparser.h"

using namespace CommandLineProcessing;

void SetupCommandlineParser(ArgvParser& cmd, int argc, char* argv[]) {
  cmd.setIntroductoryDescription("Creates lists of candidate image pairs to match from bundle file");

  //define error codes
  cmd.addErrorCode(0, "Success");
  cmd.addErrorCode(1, "Error");

  cmd.setHelpOption("h", "help",""); 
  cmd.defineOption("base_dir", "Path to base directory that stores all list files \n--list_keys.txt\n--list_images.txt\n--image_dims.txt\nnum_sifts.txt", 
      ArgvParser::OptionRequired);
  cmd.defineOption("bundle_dir", "Path to base directory that stores bundle file", 
      ArgvParser::OptionRequired);

  cmd.defineOption("result_dir", "Path to base directory that stores all matches files", 
      ArgvParser::OptionRequired);
  cmd.defineOption("nn_images", "Number of nearby images to use for matching", ArgvParser::OptionRequired); 
  cmd.defineOption("num_lists", "Number of lists to divide the pairs in", ArgvParser::OptionRequired); 
  cmd.defineOption("matches_file", "Path with filename to store the match-graph", ArgvParser::NoOptionAttribute); 
  
  cmd.defineOption("query_list", "List of image idx to use as query", ArgvParser::NoOptionAttribute); 
  /// finally parse and handle return codes (display help etc...)
  int result = cmd.parse(argc, argv);
  if (result != ArgvParser::NoParserError)
  {
    cout << cmd.parseErrorDescription(result);
    exit(-1);
  }
}

int main(int argc, char* argv[]) {
    printf("\nCreateGuidedMatchPairs : Running");
    ArgvParser cmd;
    SetupCommandlineParser(cmd, argc, argv);

    string bundleDir = cmd.optionValue("bundle_dir");
    string baseDir = cmd.optionValue("base_dir");
    string resultDir = cmd.optionValue("result_dir");

    string matchesFile, pairsFile;
    if(cmd.foundOption("matches_file")) {
        matchesFile = cmd.optionValue("matches_file");
    } else {
        matchesFile = baseDir + "bundle.matches.txt";
    }

    string numNear = cmd.optionValue("nn_images");
    int numNearby = atoi( numNear.c_str() );
    
    string numListsStr = cmd.optionValue("num_lists");
    int numLists = atoi( numListsStr.c_str() );

    sfm::MatchPairs mg(bundleDir, baseDir, numNearby);
    if(!mg.isInitialized()) {
        printf("\nNot Initialized Properly");
        fflush(stdout);
        return -1;
    }

    bool tryHard = true;
    if(!cmd.foundOption("query_list")) {
        mg.findAllCandidateImages( tryHard );
    } else {
        string queryFile = cmd.optionValue("query_list");
        mg.findSelectedCandidateImages( queryFile, tryHard ); 
    }
    int numPairs = mg.findUniquePairs();
    printf("\nFound %d Unique Pairs out of %d total pairs %d candidate pairs\n", numPairs,  mg.getTotalPairs(), mg.getTotalCandidatePairs());
    fflush(stdout);

    mg.writeUniquePairs( resultDir, numLists );
    return 0;
}

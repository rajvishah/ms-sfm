#include "defs.h"
#include "Matcher.h"
#include "sifts.h"
#include "Gridder.h"

void find_matches(char *argv[])
{
#ifdef SIFTGPU_DLL_RUNTIME
    #ifdef _WIN32
        #ifdef _DEBUG
            HMODULE  hsiftgpu = LoadLibrary("siftgpu_d.dll");
        #else
            HMODULE  hsiftgpu = LoadLibrary("/home/vanshika/vyom/Work/SiftGPU/bin/siftgpu.dll");
        #endif
    #else
        void * hsiftgpu = dlopen("/home/vanshika/vyom/Work/SiftGPU/bin/libsiftgpu.so", RTLD_LAZY);
    #endif

    if(hsiftgpu == NULL) return 0;

    #ifdef REMOTE_SIFTGPU
        ComboSiftGPU* (*pCreateRemoteSiftGPU) (int, char*) = NULL;
        pCreateRemoteSiftGPU = (ComboSiftGPU* (*) (int, char*)) GET_MYPROC(hsiftgpu, "CreateRemoteSiftGPU");
        ComboSiftGPU * combo = pCreateRemoteSiftGPU(REMOTE_SERVER_PORT, REMOTE_SERVER);
        SiftGPU* sift = combo;
        SiftMatchGPU* matcher = combo;
    #else
        SiftGPU* (*pCreateNewSiftGPU)(int) = NULL;
        SiftMatchGPU* (*pCreateNewSiftMatchGPU)(int) = NULL;
        pCreateNewSiftGPU = (SiftGPU* (*) (int)) GET_MYPROC(hsiftgpu, "CreateNewSiftGPU");
        pCreateNewSiftMatchGPU = (SiftMatchGPU* (*)(int)) GET_MYPROC(hsiftgpu, "CreateNewSiftMatchGPU");
        SiftGPU* sift = pCreateNewSiftGPU(1);
        SiftMatchGPU* matcher = pCreateNewSiftMatchGPU(4096);
    #endif

#elif defined(REMOTE_SIFTGPU)
    ComboSiftGPU * combo = CreateRemoteSiftGPU(REMOTE_SERVER_PORT, REMOTE_SERVER);
    SiftGPU* sift = combo;
    SiftMatchGPU* matcher = combo;
#else
    //this will use overloaded new operators
    SiftGPU  *sift = new SiftGPU;
    SiftMatchGPU *matcher = new SiftMatchGPU(4096);
#endif
    std::vector<float > descriptors1(1), descriptors2(1);
    std::vector<SiftGPU::SiftKeypoint> keys1(1), keys2(1);    
    int num1 = 0, num2 = 0;

//    if(sift->CreateContextGL() != SiftGPU::SIFTGPU_FULL_SUPPORTED) return 0;

    if(sift->RunSIFT(argv[1]))
    {
        //Call SaveSIFT to save result to file, the format is the same as Lowe's
        //sift->SaveSIFT("../data/800-1.sift"); //Note that saving ASCII format is slow

        //get feature count
        num1 = sift->GetFeatureNum();

        //allocate memory
        keys1.resize(num1);    descriptors1.resize(128*num1);

        //reading back feature vectors is faster than writing files
        //if you dont need keys or descriptors, just put NULLs here
        sift->GetFeatureVector(&keys1[0], &descriptors1[0]);
        //this can be used to write your own sift file.            
    }

    //You can have at most one OpenGL-based SiftGPU (per process).
    //Normally, you should just create one, and reuse on all images. 
    if(sift->RunSIFT(argv[2]))
    {
        num2 = sift->GetFeatureNum();
        keys2.resize(num2);    descriptors2.resize(128*num2);
        sift->GetFeatureVector(&keys2[0], &descriptors2[0]);
    }
    matcher->SetLanguage(SiftMatchGPU::SIFTMATCH_CUDA); // +i for the (i+1)-th device
    matcher->VerifyContextGL(); //must call once
    matcher->SetDescriptors(0, num1, &descriptors1[0]); //image 1
    matcher->SetDescriptors(1, num2, &descriptors2[0]); //image 2
    int (*match_buf)[2] = new int[num1][2];
  //  int num_match = matcher->GetSiftMatch(num1, match_buf);
	sift_img source;
	sift_img target;

//	std::vector<float > descriptors1(1), descriptors2(1);
//	std::vector<SiftGPU::SiftKeypoint> keys1(1), keys2(1);

//	source.SetSift(argv[1],keys1,descriptors1);
//	target.SetSift(argv[2],keys2,descriptors2);
	source.init(num1,argv[1]);
	target.init(num2,argv[2]);
//	source.ReadSiftFile(argv[1],argv[4]);
//	target.ReadSiftFile(argv[2],argv[5]);
	FILE *fMatrix = fopen ( argv[3] , "r" );
	float f_matrix[9];

	fscanf(fMatrix,"%f%f%f%f%f%f%f%f%f",&f_matrix[0],&f_matrix[1],&f_matrix[2],&f_matrix[3],&f_matrix[4],&f_matrix[5],&f_matrix[6],&f_matrix[7],&f_matrix[8]); 

	fclose (fMatrix);

	Matcher new_matcher;
	new_matcher.matchImagePair(source,target,f_matrix,keys1,descriptors1,keys2,descriptors2);

	new_matcher.visualizeMatches(source,target,new_matcher.matches,keys1,keys2);

    // clean up..
    delete[] match_buf;
#ifdef REMOTE_SIFTGPU
    delete combo;
#else
    delete sift;
    delete matcher;
#endif

}

int main(int argc,char *argv[])
{
	if(argc!=4)
	{
		printf("To run the program, type ./a.out source.key target.key fmatrix\n");
		return 0;
	}

	find_matches(argv);
	return 0;
}

#include "defs.h"
#include "Matcher.h"
#include "sifts.h"
#include "Gridder.h"

void siftgpuMatchingwithKey(char *argv[])
{
#ifdef SIFTGPU_DLL_RUNTIME
    #ifdef _WIN32
        #ifdef _DEBUG
            HMODULE  hsiftgpu = LoadLibrary("/home/vanshika/vyom/Work/SiftGPU/bin/siftgpu_d.dll");
        #else
            HMODULE  hsiftgpu = LoadLibrary("/home/vanshika/vyom/Work/SiftGPU/bin/siftgpu.dll");
        #endif
    #else
        void * hsiftgpu = dlopen("/home/rajvi/SiftGPU/bin/libsiftgpu.so", RTLD_LAZY);
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
    

    //Create a context for computation, and SiftGPU will be initialized automatically 
    //The same context can be used by SiftMatchGPU
   // if(sift->CreateContextGL() != SiftGPU::SIFTGPU_FULL_SUPPORTED) return 0;

    matcher->SetLanguage(SiftMatchGPU::SIFTMATCH_CUDA); // +i for the (i+1)-th device

    // my descriptors and keys here
    std::vector<std::string> list;
    std::ifstream in_stream;
    std::string line;
    in_stream.open(argv[1]);

    while(!in_stream.eof())
    {
	    in_stream >> line;
	    list.push_back(line);
    }

    in_stream.close();

    std::string new1,new2;
	
    std::vector<float > descriptors1(1), descriptors2(1);
    std::vector<SiftGPU::SiftKeypoint> keys1(1), keys2(1);    
    int num1 = 0, num2 = 0;
    std::vector<float > mydescriptors1(1), mydescriptors2(1);
    std::vector<SiftGPU::SiftKeypoint> mykeys1(1), mykeys2(1);    
    int mynum1 = 0, mynum2 = 0,desc_size = 0;
    
    FILE *img1,*img2;
    std::ofstream myfile;
    
    myfile.open (argv[2],std::ofstream::app);

    for(int id = 0 ; id < list.size()-1; id++)
    {
	    new1 = list[id];
	    new1.replace(new1.size()-3,3,"key");
	    
	    img1 = fopen((char *)new1.c_str(),"r");
	    
	    fscanf(img1,"%d",&mynum1);
	    fscanf(img1,"%d",&desc_size);
	    mykeys1.resize(mynum1/5);    mydescriptors1.resize(128*mynum1/5);
	    for(int i = 0; i < mynum1/5; i++)
	    {
		    fscanf(img1,"%f%f%f%f",&mykeys1[i].y,&mykeys1[i].x,&mykeys1[i].s,&mykeys1[i].o);
		    for(int j = 0; j < 128; j++)
		    {
			    fscanf(img1,"%f",&mydescriptors1[(i*128)+j]);
			    mydescriptors1[(i*128)+j] = (mydescriptors1[(i*128)+j])/512;
		    }

	    }
	    for(int j = id+1; j < list.size()-1; j++)
	    {
		    new2 = list[j];
		    new2.replace(new2.size()-3,3,"key");

		    img2 = fopen((char *)new2.c_str(),"r");

		    fscanf(img2,"%d",&mynum2);
		    fscanf(img2,"%d",&desc_size);


		    int num1,num2;
		    if(mynum1 < mynum2)
		    {  
			    num1 = mynum1/10;
			    num2 = mynum2/5;

		    }
		    else
		    {
			    num1 = mynum2/10;
			    num2 = mynum1/5;
		    }

		    bool allQueriesDone = false;
		    bool allRefDone = false;
		    if(num1 < 100)
		    {
			    allQueriesDone = true;
			    if(mynum1 < mynum2)
				    num1 = mynum1;
			    else
				    num1 = mynum2;
		    }
		    else if(num1 < 1000)
			    num1 = 1000;
		    if(num2 < 100)
		    {
			    allRefDone = true;
			    if(mynum1 < mynum2)
				    num2 = mynum2;
			    else
				    num2 = mynum1;
		    }
		    else if(num2 < 1000)
			    num2 = 1000;


		    mykeys2.resize(mynum2/5);    mydescriptors2.resize(128*mynum2/5);

		    for(int i = 0; i < mynum2/5; i++)
		    {
			    fscanf(img2,"%f%f%f%f",&mykeys2[i].y,&mykeys2[i].x,&mykeys2[i].s,&mykeys2[i].o);
			    for(int j = 0; j < 128; j++)
			    {
				    fscanf(img2,"%f",&mydescriptors2[(i*128)+j]);
				    mydescriptors2[(i*128)+j] = (mydescriptors2[(i*128)+j])/512;
			    }

		    }
		    fclose(img2);

		    matcher->VerifyContextGL(); //must call once

		    int max_sift = num1;

		    matcher->SetMaxSift(max_sift);
		    if(mynum1 < mynum2)
		    {
			    matcher->SetDescriptors(0, num1, &mydescriptors1[0]); //image 1
			    matcher->SetDescriptors(1, num2, &mydescriptors2[0]); //image 2
		    }
		    else
		    {
			    matcher->SetDescriptors(1, num2, &mydescriptors1[0]); //image 1
			    matcher->SetDescriptors(0, num1, &mydescriptors2[0]); //image 2
		    }

		    //match and get result.    
		    int (*match_buf)[2] = new int[max_sift][2];
		    int num_match = matcher->GetSiftMatch(max_sift, match_buf);

		    if(num_match < 4)
			    continue;
		    if(num_match >= 200)
		    {
			    if(mynum1 < mynum2)
				    myfile <<  list[id] << " " << list[j] << " " << num_match <<  "\n" ;
			    else
				    myfile <<  list[j] << " " << list[id] << " " << num_match <<  "\n" ;
			    for(int i = 0 ; i < num_match; i++)
				    myfile << match_buf[i][0] << " ";
			    myfile << "\n";
			    for(int i = 0 ; i < num_match; i++)
				    myfile << match_buf[i][1] << " ";
			    myfile << "\n";

		    }
		    else
		    {
			    max_sift*=2;
			    matcher->SetMaxSift(max_sift);
			    if(mynum1 < mynum2)
			    {
				    matcher->SetDescriptors(0, mynum1/5, &mydescriptors1[0]); //image 1
				    matcher->SetDescriptors(1, mynum2/5, &mydescriptors2[0]); //image 2
			    }
			    else
			    {
				    matcher->SetDescriptors(1, mynum1/5, &mydescriptors1[0]); //image 1
				    matcher->SetDescriptors(0, mynum2/5, &mydescriptors2[0]); //image 2
			    }


			    match_buf = new int[max_sift][2];
			    num_match = matcher->GetSiftMatch(max_sift, match_buf);


			    if(num_match >= 200)
			    {
				    if(mynum1 < mynum2)
					    myfile <<  list[id] << " " << list[j] << " " << num_match <<  "\n" ;
				    else
					    myfile <<  list[j] << " " << list[id] << " " << num_match <<  "\n" ;
				    for(int i = 0 ; i < num_match; i++)
					    myfile << match_buf[i][0] << " ";
				    myfile << "\n";
				    for(int i = 0 ; i < num_match; i++)
					    myfile << match_buf[i][1] << " ";
				    myfile << "\n";

			    }
		    }
		    delete[] match_buf;
	    }
	    fclose(img1);
    }

    myfile.close();
    /*
    while(num_match < 16)
    {
	    max_sift*=2;
	    matcher->SetMaxSift(max_sift);
	    matcher->SetDescriptors(0, max_sift, &mydescriptors1[0]); //image 1
	    matcher->SetDescriptors(1, max_sift, &mydescriptors2[0]); //image 2
    	    match_buf = new int[max_sift][2];
	    num_match = matcher->GetSiftMatch(max_sift, match_buf);
	    if(max_sift >= num1)
		    break;
    }

    if(num_match >= 16)
    {
	    std::ofstream myfile;
	    myfile.open (file,std::ofstream::app);
	    if(mynum1 < mynum2)
		    myfile <<  image1 << " " << image2 << " " << num_match <<  "\n" ;
	    else
		    myfile <<  image2 << " " << image1 << " " << num_match <<  "\n" ;
	    for(int i = 0 ; i < num_match; i++)
		    myfile << match_buf[i][0] << " ";
	    myfile << "\n";
	    for(int i = 0 ; i < num_match; i++)
		    myfile << match_buf[i][1] << " ";
	    myfile << "\n";

	    myfile.close();
    }
    */
    // clean up..
#ifdef REMOTE_SIFTGPU
    delete combo;
#else
    delete sift;
    delete matcher;
#endif

#ifdef SIFTGPU_DLL_RUNTIME
    FREE_MYLIB(hsiftgpu);
#endif
 

}
void initial_matching(char *image1,char *image2,char *file_name)
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

    if(sift->RunSIFT(image1))
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
    if(sift->RunSIFT(image2))
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
 	 //if you want to use more than 4096 or less than 4096
       //call matcher->SetMaxSift() to change the limit before calling setdescriptor

    int max_sift = num1/10;
    if(max_sift < 1000)
	    max_sift = 1000;
	matcher->SetMaxSift(max_sift);
    int num_match = matcher->GetSiftMatch(num1, match_buf);

	while(num_match < 16)
	{
		max_sift*=2;
		matcher->SetMaxSift(max_sift);
		matcher->SetDescriptors(0, num1, &descriptors1[0]); //image 1
		matcher->SetDescriptors(1, num2, &descriptors2[0]); //image 2
		num_match = matcher->GetSiftMatch(num1, match_buf);
	}

	std::ofstream myfile;
	myfile.open (file_name,std::ofstream::app);
	myfile <<  image1 << " " << image2 << " " << num_match <<  "\n" ;
	for(int i = 0 ; i < num_match; i++)
		myfile << match_buf[i][0] << " ";
	myfile << "\n";
	for(int i = 0 ; i < num_match; i++)
		myfile << match_buf[i][1] << " ";
	myfile << "\n";
    
	myfile.close();
// clean up..
    delete[] match_buf;
#ifdef REMOTE_SIFTGPU
    delete combo;
#else
    delete sift;
    delete matcher;
#endif

}

/*
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
 	 //if you want to use more than 4096 or less than 4096
       //call matcher->SetMaxSift() to change the limit before calling setdescriptor

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
*/

void find_matches1(char *imag1,char *imag2,char *sift1,char *sift2,float f_matrix[],char * file,int id1,int id2)
{
	sift_img source;
	sift_img target;


 
    std::vector<float > mydescriptors1(1), mydescriptors2(1);
    std::vector<SiftGPU::SiftKeypoint> mykeys1(1), mykeys2(1);    
    int mynum1 = 0, mynum2 = 0,desc_size = 0;
    FILE *img1,*img2;
    img1 = fopen(sift1,"r");
    img2 = fopen(sift2,"r");
    
    fscanf(img1,"%d",&mynum1);
    fscanf(img1,"%d",&desc_size);
    fscanf(img2,"%d",&mynum2);
    fscanf(img2,"%d",&desc_size);
    mykeys1.resize(mynum1);    mydescriptors1.resize(128*mynum1);
    mykeys2.resize(mynum2);    mydescriptors2.resize(128*mynum2);
    for(int i = 0; i < mynum1; i++)
    {
    	fscanf(img1,"%f%f%f%f",&mykeys1[i].y,&mykeys1[i].x,&mykeys1[i].s,&mykeys1[i].o);
	for(int j = 0; j < 128; j++)
	{
    		fscanf(img1,"%f",&mydescriptors1[(i*128)+j]);
    		mydescriptors1[(i*128)+j] = (mydescriptors1[(i*128)+j])/512;
	}

    }
    for(int i = 0; i < mynum2; i++)
    {
    	fscanf(img2,"%f%f%f%f",&mykeys2[i].y,&mykeys2[i].x,&mykeys2[i].s,&mykeys2[i].o);
	for(int j = 0; j < 128; j++)
	{
    		fscanf(img2,"%f",&mydescriptors2[(i*128)+j]);
    		mydescriptors2[(i*128)+j] = (mydescriptors2[(i*128)+j])/512;
	}

    }
    fclose(img1);
    fclose(img2);


	source.init(mynum1,imag1);
	target.init(mynum2,imag2);


	Matcher new_matcher;
	new_matcher.matchImagePair(source,target,f_matrix,mykeys1,mydescriptors1,mykeys2,mydescriptors2);

//	new_matcher.visualizeMatches(source,target,new_matcher.matches,keys1,keys2);
}


void find_matches(char *img1,char *img2,char *key1,char *key2,float *fmatrix,char * file,int id1,int id2)
{
	int num1 = 0, num2 = 0;

	sift_img source;
	sift_img target;

	std::vector<float > descriptors1(1), descriptors2(1);
	std::vector<SiftGPU::SiftKeypoint> keys1(1), keys2(1);

	source.SetSift(key1,keys1,descriptors1);
	target.SetSift(key2,keys2,descriptors2);
	source.init(source.num_keys,img1);
	target.init(target.num_keys,img2);

	Matcher new_matcher;
	try{
	new_matcher.matchImagePair(source,target,fmatrix,keys1,descriptors1,keys2,descriptors2);
	}
	catch(thrust::system_error e){
		std::cout << "error\n" ;
	}
	
	catch( std::bad_alloc &e){
		std::cout << "error\n" ;
	}
	catch(thrust::system::detail::bad_alloc &e){
		std::cout << "error\n" ;
	}
	catch(thrust::system::system_error &e){
		std::cout << "error\n" ;
	}
	
	
	
	std::ofstream myfile;
	myfile.open (file,std::ofstream::app);

	for(int i = 0 ; i < new_matcher.matches.size(); i++)
	{
		int sid1 = new_matcher.matches[i].first ;
		int sid2 = new_matcher.matches[i].second ;
		myfile << id1 << " " << sid1 << " " << keys1[sid1].x-(source.width/2) << " " << (source.height/2)-keys1[sid1].y << " " << id2 << " " << sid2 << " " << keys2[sid2].x -(target.width/2)<< " " << (target.height/2)-keys2[sid2].y << "\n";
	}

		myfile.close();

//	new_matcher.visualizeMatches(source,target,new_matcher.matches,keys1,keys2);

}

int main(int argc,char *argv[])
{
	if(argc != 3 && argc != 5)
	{
		printf("To run the program, type ./a.out intput_images_list output_file\n");
		return 0;
	}

	if(argc == 3)
	{
			siftgpuMatchingwithKey(argv);//(char *)list[i].c_str(),(char *)list[j].c_str(),(char *)new1.c_str(),(char *)new2.c_str(), argv[2]);
				//			initial_matching((char *)list[i].c_str(),(char *)list[j].c_str(),argv[2]);
	}
	else if(argc == 5)
	{

		std::vector<std::string> list_img;
		std::ifstream img_files;
		std::string line;
		img_files.open(argv[1]);

		while(!img_files.eof())
		{
			img_files >> line;
			list_img.push_back(line);
		}

		img_files.close();

		std::vector<std::string> list_key;
		std::ifstream key_files;
		key_files.open(argv[2]);

		while(!key_files.eof())
		{
			key_files >> line;
			list_key.push_back(line);
		}

		key_files.close();


		std::ifstream bundler;
		bundler.open (argv[3]);

		float f_matrix[9];
		int id1,id2;
		int i = 1;
	
		while(!bundler.eof())
		{
			bundler >> id1 >> id2 >> f_matrix[0] >> f_matrix[1] >> f_matrix[2] >> f_matrix[3] >> f_matrix[4] >> f_matrix[5] >> f_matrix[6] >> f_matrix[7] >> f_matrix[8] ; 
		
			std::cout << i << " " << id1 << " " << id2 << "\n";
			i++;
			find_matches((char *)list_img[id1].c_str(),(char *)list_img[id2].c_str(),(char *)list_key[id1].c_str(),(char *)list_key[id2].c_str(),f_matrix,argv[4],id1,id2);

		}
		
		//		find_matches1((char *)list_img[id1].c_str(),(char *)list_img[id2].c_str(),(char *)list_key[id1].c_str(),(char *)list_key[id2].c_str(),f_matrix,argv[4],id1,id2);
		bundler.close();	




	}
	return 0;
}

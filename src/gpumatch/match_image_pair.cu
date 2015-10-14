#include "defs.h"
#include "Matcher.h"
#include "sifts.h"
#include "Gridder.h"

int main(int argc,char *argv[])
{

  if(argc == 5) //Do match-graph construction for given list
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



		std::vector<sift_img> allSifts(list_key.size());
		std::vector<std::vector<unsigned char> > allDescriptors(list_key.size(),std::vector<unsigned char> (1));
		std::vector<std::vector<SiftGPU::SiftKeypoint> > allKeys(list_key.size(),std::vector<SiftGPU::SiftKeypoint> (1));


		SiftGPU::SiftKeypoint **h_allKeys = (SiftGPU::SiftKeypoint **)malloc(sizeof(SiftGPU::SiftKeypoint*)*list_key.size());
		unsigned char **h_allDesc = (unsigned char **)malloc(sizeof(unsigned char *) * list_key.size());

		std::cout << list_key.size() << std::endl;

		for(int i = 0 ; i < list_key.size(); i++)
		{
			std::cout << list_key[i] << "\n" ; 
			allSifts[i].SetSift((char *)list_key[i].c_str(),allKeys[i],allDescriptors[i]);
			allSifts[i].init(allSifts[i].num_keys,(char *)list_img[i].c_str());
			cudaMalloc((void **)&h_allKeys[i],allSifts[i].num_keys*sizeof(SiftGPU::SiftKeypoint));
			cudaMemcpy(h_allKeys[i], allKeys[i].data(), sizeof(SiftGPU::SiftKeypoint)*allSifts[i].num_keys, cudaMemcpyHostToDevice); 
			cudaMalloc((void **)&h_allDesc[i],allSifts[i].num_keys*128*sizeof(unsigned char));
			cudaMemcpy(h_allDesc[i], allDescriptors[i].data(), sizeof(unsigned char)*allSifts[i].num_keys*128, cudaMemcpyHostToDevice); 
			printf("reading id %d\n",i);
		}

		unsigned char **d_allDesc;
		cudaMalloc((void **)&d_allDesc,sizeof(unsigned char*) * list_key.size());

		unsigned char **d_allKeys;
		cudaMalloc((void **)&d_allKeys,sizeof(SiftGPU::SiftKeypoint*) * list_key.size());

		cudaMemcpy(d_allDesc, h_allDesc, sizeof(unsigned char*) * list_key.size(), cudaMemcpyHostToDevice);
		cudaMemcpy(d_allKeys, h_allKeys, sizeof(SiftGPU::SiftKeypoint*) * list_key.size(), cudaMemcpyHostToDevice);

		std::ifstream bundler;
		bundler.open (argv[3]);

		float f_matrix[9];
		int id1,id2;
		int i = 1;
		double ttime = 0;
		
		std::ofstream myfile;
		myfile.open (argv[4]);

		while(!bundler.eof())
		{
			bundler >> id1 >> id2 >> f_matrix[0] >> f_matrix[1] >> f_matrix[2] >> f_matrix[3] >> f_matrix[4] >> f_matrix[5] >> f_matrix[6] >> f_matrix[7] >> f_matrix[8] ; 
		
			std::cout << i << " " << id1 << " " << id2 << "\n";
			i++;
			Matcher new_matcher;

			ttime += new_matcher.matchImagePair(allSifts[id1],allSifts[id2],f_matrix,h_allKeys[id1],h_allDesc[id1],h_allKeys[id2],h_allDesc[id2]);	
	
			for(int i = 0 ; i < new_matcher.matches.size(); i++)
			{
				int sid1 = new_matcher.matches[i].first ;
				int sid2 = new_matcher.matches[i].second ;
				if(id1 >= list_key.size() || id2 >= list_key.size())
					printf("image id invalid\n");
				else if(sid1 >= allSifts[id1].num_keys)
					printf("%d %d\n",id1,sid1);
				else if (sid2 >= allSifts[id2].num_keys)
					printf("%d %d\n",id2,sid2);
				myfile << id1 << " " << sid1 << " " << allKeys[id1][sid1].x-(allSifts[id1].width/2) << " " << (allSifts[id1].height/2)-allKeys[id1][sid1].y << " " << id2 << " " << sid2 << " " << allKeys[id2][sid2].x -(allSifts[id2].width/2)<< " " << (allSifts[id2].height/2)-allKeys[id2][sid2].y << "\n";
			}
			std::cout << "Total time : " << ttime << std::endl;
			std::cout << "found\n" ;

		}

		myfile.close();
		bundler.close();	
		std::cout << "Total time : " << ttime << std::endl;



	}
	return 0;
}

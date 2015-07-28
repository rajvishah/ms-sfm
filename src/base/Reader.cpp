#include "Reader.h"
#include "matrix.h"
#include "Bundle.h"
#include "Geometric.h"

using namespace reader;

ImageAttr::ImageAttr(string n, int w, int h) {
	name = n;
	width = w;
	height = h;
}

ImageListReader::ImageListReader(string& baseDir) {
	listFileName = baseDir + string("/list_images.txt");
	dimsFileName = baseDir + string("/image_dims.txt");
}

void ImageListReader::initialize(string& baseDir) {
	listFileName = baseDir + string("/list_images.txt");
	dimsFileName = baseDir + string("/image_dims.txt");
}

bool ImageListReader::read() {
	ifstream imgFile(listFileName.c_str(), ifstream::in);
	ifstream dimsFile(dimsFileName.c_str(), ifstream::in);
	if( !imgFile.is_open() || ! dimsFile.is_open()) {
        cout << "\nError opening file (images/dimensions)";
		return false;
	}

    string line;
    vector<string> imgFileNames;
    while(getline(imgFile, line)) {
        imgFileNames.push_back(line);
    }

    int numImages = imgFileNames.size();
    vector< int > widths( numImages );
    vector< int > heights( numImages );
    for(int i=0; i < imgFileNames.size(); i++) {
        dimsFile >> widths[i] >> heights[i];
    } 

    for(int i=0; i < numImages; i++) {
		ImageAttr att(imgFileNames[i], widths[i], heights[i]);
		imAttr.push_back(att);			
	}

	imgFile.close();
  dimsFile.close();
	return true;
}

KeyListReader::KeyListReader(string& baseDir) {	
	keyFileName = baseDir + string("/list_keys.txt");
}

void KeyListReader::initialize(string& baseDir){	
	keyFileName = baseDir + string("/list_keys.txt");
}

bool KeyListReader::read() {
	ifstream file(keyFileName.c_str(), ifstream::in);
	if( ! file.is_open() ) {
        printf("\nError opening key list file");
		return false;
	}

	keyNames.clear();
    string line;
    while(getline(file, line)) {	
		keyNames.push_back(line);			
	}
	file.close();
	return true;
}
	
NSiftReader::NSiftReader(string& baseDir) { 
	nSiftFileName = baseDir + string("/num_sifts.txt");
}

void NSiftReader::initialize(string& baseDir) {
	nSiftFileName = baseDir + string("/num_sifts.txt");
}

bool NSiftReader::read() {
	ifstream file(nSiftFileName.c_str(), ifstream::in);
	if( ! file.is_open() ) {
        printf("\nError opening num sift file");
		return false;
	}

	numSifts.clear();
    string line;
	while(getline(file, line)) {
		int n = atoi ( line.c_str() );	
		numSifts.push_back(n);			
	}
	file.close();
	return true;
}

BundleReader::BundleReader(string& baseDir, 
		bundle::Bundle *bund) {
	bundleFileName = baseDir + string("/bundle.out");
	viewIntersFileName = baseDir + string("/view_inters.txt");
	angleArrFileName = baseDir + string("/angle_arr.txt");
	bdl = bund;
}

BundleReader::BundleReader(string& baseDir) {
	bundleFileName = baseDir + string("/bundle.out");
	viewIntersFileName = baseDir + string("/view_inters.txt");
	angleArrFileName = baseDir + string("/angle_arr.txt");
	bdl = NULL;
}

void BundleReader::initialize(string& baseDir) {
	bundleFileName = baseDir + string("/bundle.out");
	viewIntersFileName = baseDir + string("/view_inters.txt");
	angleArrFileName = baseDir + string("/angle_arr.txt");
	bdl = NULL;
}

bool BundleReader::readViewInters() {
	cout << "Reading view inters file " << viewIntersFileName.c_str() << endl;
    
    FILE *f = fopen( viewIntersFileName.c_str(), "r" );
	if( f == NULL ) {
		return false;
	}
	
	bdl->corrMat.resize( bdl->numImgs );
	for(int i=0; i < bdl->numImgs; i++) {
		bdl->corrMat[i].resize(bdl->numImgs, 0);
		int val;
		for(int j=0; j < bdl->numImgs; j++) {
			fscanf(f,"%d",&val);
			bdl->corrMat[i][j] = val;
			//printf("\nCORR:%d,%d = %d", i, j, val);
		}	
	}
	fclose(f);
	return true;
}

bool BundleReader::readAngleArr() {
	cout << "Reading angle array file " << angleArrFileName.c_str() << endl;
	
    FILE *f = fopen( angleArrFileName.c_str(), "r" );
	if( f == NULL ) {
		return false;
	}
	
	bdl->angleMat.resize( bdl->numImgs );
	for(int i=0; i < bdl->numImgs; i++) {
		bdl->angleMat[i].resize(bdl->numImgs, -1);
		float val;
		for(int j=0; j < bdl->numImgs; j++) {
			fscanf(f,"%f",&val);
			bdl->angleMat[i][j] = val;
		}	
	}
	fclose(f);
	return true;
}

void BundleReader::readHeader(FILE* f) {
	char first_line[256];
	fgets(first_line, 256, f);
	if (first_line[0] == '#') {
		sscanf(first_line, "# Bundle file v%lf", &version);
		fscanf(f, "%d %d\n", &bdl->numImgs, &bdl->numPoints);
	} else if (first_line[0] == 'v') {
		sscanf(first_line, "v%lf", &version);
		fscanf(f, "%d %d\n", &bdl->numImgs, &bdl->numPoints);
	} else {
		version = 0.1;
		sscanf(first_line, "%d %d\n", &bdl->numImgs, &bdl->numPoints);
	}
}

bool BundleReader::read(bundle::Bundle *myBdl) {
	bdl = myBdl;
	bool status = read();
	return status;
}


bool BundleReader::read() {
    if(bdl == NULL) {
		cout << "\nInvalid bundle pointer";
		return false;
	}

	FILE *f = fopen(bundleFileName.c_str(), "r");
	if (f == NULL) {
		return false;
	}
	readHeader(f);

	bool viFlag = readViewInters();
	bool aaFlag = readAngleArr();

	FILE *viFile = NULL;
	FILE *aaFile = NULL;
 
	if(!viFlag) {
        cout << "Writing view inters file\n" << endl;
		bdl->corrMat.resize( bdl->numImgs );
		viFile = fopen(viewIntersFileName.c_str(), "w");
	}

	if(!aaFlag) {
        cout << "Writing angle arr file\n" << endl;
		aaFile = fopen(angleArrFileName.c_str(), "w");
	}


	bdl->validTriangulated.resize(bdl->numImgs, false);
	bdl->camSet.resize(bdl->numImgs);
	bdl->camLocs.resize(bdl->numImgs);
	bdl->camPos.resize(bdl->numImgs);
	bdl->kf.resize(bdl->numImgs);
	bdl->P_orig.resize(bdl->numImgs);
	bdl->k_inv.resize(bdl->numImgs);


	for(int i=0; i < bdl->numImgs; i++) {
		double focalLength;
		//double R[9];
		double t[3];
		double k[2] = { 0.0, 0.0 };

		if(version >= 0.4) {
			char name[2];
			int w, h;
			fscanf(f, "%s %d %d\n", name, &w, &h);
		}
	
		/* Focal length */
		if (version > 0.1) {
			fscanf(f, "%lf %lf %lf\n", &focalLength, k+0, k+1);
		} else {
			fscanf(f, "%lf\n", &focalLength);
		}

		bdl->kf[i].el[0] = focalLength;
		bdl->kf[i].el[1] = k[0];
		bdl->kf[i].el[2] = k[1];


		double* rPtr = bdl->camSet[i].R;
		/* Rotation */
		fscanf(f, "%lf %lf %lf\n%lf %lf %lf\n%lf %lf %lf\n",
				rPtr+0, rPtr+1, rPtr+2, 
				rPtr+3, rPtr+4, rPtr+5, 
				rPtr+6,rPtr+7, rPtr+8);

		/*Zero Test for Rotation Matrix */
		/*Images that are not triangulated with 10% sift 
 * 	have zero vals in bundle file for Rotation matrix  
 * 	These should not be part of nearbyImg list hence */
		bool zeroFlag = true;
		for(int r=0; r < 9; r++) {
			if(*(rPtr+r) != 0.0f) {
				zeroFlag = false;
				break;
			}
		}

		if(zeroFlag) {
			bdl->validTriangulated[i] = false;
			focalLength = -1;
		} else {

			bdl->validTriangulated[i] = true;
		}

		/* Translation */
		fscanf(f, "%lf %lf %lf\n", &t[0],&t[1],&t[2]);
		t[0] *= -1;
		t[1] *= -1;
		t[2] *= -1;

		bdl->camPos[i].el[0] = t[0];	
		bdl->camPos[i].el[1] = t[1];	
		bdl->camPos[i].el[2] = t[2];

		bdl->camSet[i].krd_inv[0] = bdl->camSet[i].krd_inv[2] = bdl->camSet[i].krd_inv[3] = 0.0;
		bdl->camSet[i].krd_inv[4] = bdl->camSet[i].krd_inv[5] = 0.0;
		bdl->camSet[i].krd_inv[1] = 1.0;


        /*
        double k_dist[6] = { 0.0, 1.0, 0.0, k[0], 0.0, k[1] };
        double w_2 = 0.5 * imgList.getImageWidth(i);
        double h_2 = 0.5 * imgList.getImageHeight(i);

        double max_radius = 
            sqrt(w_2 * w_2 + h_2 * h_2) / focalLength;

        //geometry::InvertDistortion(6, 6, 0.0, max_radius, 
        //    k_dist, bdl->camSet[i].krd_inv);
        //    */


		double R1[9];
		
		if(bdl->validTriangulated[i]) {
			matrix_invert(3,rPtr,R1);
			matrix_product(3,3,3,1,R1,t,bdl->camSet[i].t);
		} else {
			bdl->camSet[i].t[0] = 0.0f;
			bdl->camSet[i].t[1] = 0.0f;
			bdl->camSet[i].t[2] = 0.0f;
		}

		bdl->camLocs[i].el[0] = (float)bdl->camSet[i].t[0];
		bdl->camLocs[i].el[1] = (float)bdl->camSet[i].t[1];
		bdl->camLocs[i].el[2] = (float)bdl->camSet[i].t[2];

		double K[] = {focalLength,0,0,
			0, focalLength, 0,
			0, 0,   1};

		double K_inv[9];
		matrix_invert(3, K, K_inv);

		bdl->k_inv[i].resize(9);
		for(int i1=0; i1 < 9; i1++) {
			bdl->k_inv[i][i1] = K_inv[i1];
		}

		double RT[12];
		for(int i1=0;i1<3;i1++)
		{
			for(int j1=0;j1<3;j1++) {
				RT[i1*4+j1] = bdl->camSet[i].R[i1*3+j1];
			}
			RT[i1*4+3] = -t[i1];
		}

		bdl->P_orig[i].resize(12);
		matrix_product(3,3,3,4,K,RT,bdl->P_orig[i].data());
		for(int i1=8;i1<12;i1++)
		{
			bdl->P_orig[i][i1] = -bdl->P_orig[i][i1];	
		}

		if(!viFlag) {
			bdl->corrMat[i].resize( bdl->numImgs, 0);
		}
	}

	bdl->viewArr.resize( bdl->numImgs );
	bdl->vertexSet.resize( bdl->numPoints );
	bdl->pointArr.resize( bdl->numPoints );

	for (int i = 0; i < bdl->numPoints; i++) {

		/* Position */
		fscanf(f, "%f %f %f\n",&bdl->vertexSet[i].mPos[0], 
			&bdl->vertexSet[i].mPos[1], &bdl->vertexSet[i].mPos[2]);

		/* Color */
		fscanf(f, "%f %f %f\n",&bdl->vertexSet[i].mColor[0], 
			&bdl->vertexSet[i].mColor[1], &bdl->vertexSet[i].mColor[2]);

		bdl->vertexSet[i].mColor[0]/=255.0;
		bdl->vertexSet[i].mColor[1]/=255.0;
		bdl->vertexSet[i].mColor[2]/=255.0;

		int num_visible;
		fscanf(f, "%d", &num_visible);
		bdl->vertexSet[i].mNumVis=num_visible;

		vector<int> tempView( num_visible );
		for (int j = 0; j < num_visible; j++) {
			int view, key;
			fscanf(f, "%d %d", &view, &key);

			bdl->pointArr[i].push_back(view);
			bdl->viewArr[view].push_back(pair < int, int > (key,i));
			double x, y;
			fscanf(f, "%lf %lf", &x, &y);

			tempView[j] = view;
		}

		if(!viFlag) {
		//Fill up view inters variables - corrMat
			for(int k=0; k < num_visible; k++) {
				for(int l=0; l < num_visible; l++) {
					bdl->corrMat[tempView[k]][tempView[l]]++;
				}
			}	
		}	
	}
	fclose(f);

	if(!viFlag || !aaFlag) {
	//Fill-up angle arr var- angleMat
		for(int i=0; i < bdl->numImgs; i++) {
			vector< float > angleRow(bdl->numImgs, -1);
			for(int j=0; j < bdl->numImgs; j++) {
				if(!viFlag && viFile != NULL) {
					fprintf(viFile, "%d ", bdl->corrMat[i][j]);
				}

				if(!aaFlag) {
					if(bdl->corrMat[i][j] != 0) {
						float angle = 1.0f /* bdl->calculateAngle(i, j) */; ///DANGER!!!! 
						angleRow[j] = angle;
					}
					if(aaFile != NULL) {
						fprintf(aaFile,"%f",angleRow[j]);
					}
				}
			}

			if(aaFile != NULL) {
				fprintf(aaFile,"\n");
			}
	
			if(viFile != NULL) {
				fprintf(viFile, "\n");
			}


			if(!aaFlag) {
				bdl->angleMat.push_back(angleRow);
			}
		} 
	}

	if(NULL != viFile) fclose(viFile);
	if(NULL != aaFile) fclose(aaFile);
	return true;
}


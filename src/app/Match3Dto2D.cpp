#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "../base/Reader.h"
#include "../base/Bundle.h"
#include "../base/keys2a.h"

#include "vector.h"
#include "matrix.h"

#include "../base/Localize.h"
using namespace std;
int main( int argc, char* argv[]) {

    if(argc < 4) {
        printf("Usage : ./localize <baseDir> <level> <imageIdx>\n");
        return -1;
    }

    string imageIdxStr = argv[3];
    int imageIdx = atoi(imageIdxStr.c_str());

    string levelStr = argv[2];
    int level = atoi(levelStr.c_str());

    string listFileName = string(argv[1]) + "/coarse_model/list.txt";

    stringstream stream;
    stream << argv[1] << "/initial_matching/ranks/file-" 
        << imageIdx << ".txt.srt";
    string rankFileName = stream.str();

    string bundlePath;
    string camParPath;

    if(level==-1) {
        bundlePath = string(argv[1]) + "/coarse_model/bundle/";
    } else {
        stringstream strm, strm1;
        strm << argv[1] << "/loc" << setw(1) << level << "/sattler_loc/";
        bundlePath = strm.str();
        strm1 << argv[1] << "/loc" << setw(1) << level << "/sattler_loc/" << setfill('0') << setw(4) << imageIdx <<"-cam-par.txt";
        camParPath = strm1.str();
        cout << camParPath << endl;
        fflush(stdout);
    }

    ifstream listFile(listFileName.c_str(), std::ifstream::in);
    if(!listFile.is_open()) {
        cout << "\nError opening List File";
        return -1;
    }

    string imageString;
    vector<string> keyFileNames;
    string line;
    int line_count = 0;
    while(getline(listFile, line)) {
        if(line_count == imageIdx) {
            imageString = line;
        }
        stringstream strm(line);
        string str;
        std::getline(strm, str, '.');
        string keyFileName = str + ".key";
        keyFileNames.push_back(keyFileName);
        line_count++;
    }
    listFile.close();

    bundle::Bundle bdl;
    reader::BundleReader br(bundlePath, &bdl); 
    br.read(&bdl);

    unsigned char* refKeys;
    keypt_t* refKeysInfo;
    int numRefKeys = ReadKeyFile( keyFileNames[imageIdx].c_str(), 
            &refKeys, &refKeysInfo );

    ifstream camParFile(camParPath.c_str(), std::ifstream::in);
    if(!camParFile.is_open()) {
        cout << "\nError Opening Cam Par File";
        return -1;
    }

    vector< pair<int, int> > matches3D_2D;
    double P[12], K[9], R[9], C[3];
    if(getline(camParFile, line)) {
        stringstream strm(line);
        int numCorr = 0;
        strm >> numCorr;
        for(int c=0; c < numCorr; c++) {
            int p3Idx, keyIdx;
            camParFile >> keyIdx >> p3Idx;
            matches3D_2D.push_back(make_pair(p3Idx, keyIdx));
        }

        int row = 0;
        for(row=0; row < 3; row++) {
            for(int col=0; col < 4; col++) {
                camParFile >> P[row*4 + col];
                printf("%lf", P[row*4 + col]);
            }
            printf("\n");
        }

        getline(camParFile, line);
        for(row=0; row <3; row++) {
            for(int col=0; col<3; col++) {
                camParFile >> K[row*3 + col];
            }
        }

        getline(camParFile, line);
        for(row=0; row <3; row++) {
            for(int col=0; col<3; col++) {
                camParFile >> R[row*3 + col];
            }
        }

        for(int col=0; col<3; col++) {
            camParFile >> C[col];
        }
    }

    //printf("\n\nRotation");
    //matrix_print(3,3,R);
    //printf("\n\nCalibration");
    //matrix_print(3,3,K);
    //printf("\n\nCenter");
    //matrix_print(3,1,C);

    int neg = (K[0] < 0.0) + (K[4] < 0.0) + (K[8] < 0.0);
    if(neg == 3) {
        printf("\nAll three negative");
    } else {
        printf("\nNum negative %d", neg); 
    }

    double KR[9], t[3], Pnew[12], tmp_t[3];
    matrix_product(3,3,3,3,K,R,KR);

//    printf("\n\nKR");
//    matrix_print(3,3,KR);

    matrix_product(3,3,3,1,R,C,t);
    matrix_product(3,3,3,1,K,t,tmp_t);
    matrix_scale(3,1,t,-1.0,t);
    matrix_scale(3,1,tmp_t,-1.0,tmp_t);
    printf("\n\nTranslation");
    matrix_print(3,1,t);

    memcpy(Pnew + 0, KR + 0, 3*sizeof(double));
    memcpy(Pnew + 4, KR + 3, 3*sizeof(double));
    memcpy(Pnew + 8, KR + 6, 3*sizeof(double));

    Pnew[3] = tmp_t[0];
    Pnew[7] = tmp_t[1];
    Pnew[11] = tmp_t[2];

 //   matrix_print(3,4, P);
    matrix_print(3,4, Pnew);


    camParFile.close();
    printf("\nFound %d matches", matches3D_2D.size()); 
    vector< pair<int, int> >& finalMatches2 = matches3D_2D;

    vector< v3_t > pt3;
    vector< v2_t > pt2;
    vector< int > pt3_idx;
    vector< int > pt2_idx;

    localize::ImageData data;
    char* imageStr = strdup(imageString.c_str());
    data.InitFromString(imageStr, ".", false);
    data.m_licensed = true;
    NormalizeKeyPoints(numRefKeys, refKeysInfo, 
            data.GetWidth(), data.GetHeight());

    for(int count=0; count < finalMatches2.size(); count++) {
        pt3_idx.push_back(count);
        pt2_idx.push_back(count);

        const bundle::Vertex* vert = bdl.getVertex(finalMatches2[count].first);
        v3_t p3 = v3_new(vert->mPos[0], vert->mPos[1], vert->mPos[2]);

//        printf("\nIdx : %d", finalMatches2[count].second);

        float x = refKeysInfo[finalMatches2[count].second].nx;
        float y = refKeysInfo[finalMatches2[count].second].ny;

        v2_t p2 = v2_new(x, y);

        pt3.push_back(p3);
        pt2.push_back(p2);
    }


    printf("\nFound %d good 3D-2D correspondences\n", finalMatches2.size());

    //Prepare Structures to Register Camera
    if(finalMatches2.size() < 8) {
        printf("\nNot enough matches to initiate registration, quitting");
        return 0;
    }


    for(int i=0; i < numRefKeys; i++) {
        localize::KeypointWithDesc kpd(refKeysInfo[i].nx, refKeysInfo[i].ny, 
                refKeys + 128*i);
        data.m_keys_desc.push_back(kpd);
    } 

    double Kinit[9], Rinit[9], tinit[3];
    std::vector<int> inliers, inliers_weak, outliers;

    matrix_print(3,4,P);

    bool status = BundleRegisterImage(data, pt3, pt2, pt3_idx, pt2_idx, NULL, K, R, t);
    if(status) {
        printf("\nSuccessfully Registered Image %d", imageIdx);
        fflush(stdout);
        data.WriteCamera();
        data.WriteTracks();
    } else {
        printf("\nFailed to register image %d", imageIdx);
        fflush(stdout);
    }
    return -1;
}

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

    if(argc < 5) {
        printf("Usage : ./localize <bundlePath> <listPath> <queryImagePath> <outPath> <queryIdx>\n");
        return -1;
    }

    string bundlePath = string(argv[1]);
    string listFileName = string(argv[2]);
    string queryFileName = string(argv[3]);
    string camParPath = string(argv[4]);
    string queryIdxStr = argv[5];
    int queryIdx = atoi(queryIdxStr.c_str());

    ifstream queryFile( queryFileName.c_str(), ifstream::in);
    if( ! queryFile.is_open() ) {
        printf("\nCould not open query file");
    }

    vector<int> queryToImage;
    map<int,string> queryMap;
    string line;
    while(getline(queryFile, line)) {
        stringstream ss(line);
        int qId;
        string qName;
        ss >> qId >> qName;
        queryMap.insert(make_pair(qId, qName)); 
        queryToImage.push_back(qId);
    }
   
    int imageIdx = queryToImage[queryIdx];
    stringstream ss;
    ss << camParPath << setfill('0') << setw(4) << imageIdx <<"-cam-par.txt";
    string camFilePath = ss.str();
    
    ss.str("");
    ss << camParPath << setfill('0') << setw(4) << imageIdx <<"-cam-bdl.txt";
    string camOutPath = ss.str();
    
    ss.str("");
    ss << camParPath << setfill('0') << setw(4) << imageIdx <<"-trk-bdl.txt";
    string trackOutPath = ss.str();


    bundle::Bundle bdl;
    reader::BundleReader br(bundlePath, &bdl); 
    br.read(&bdl);

    ifstream listFile(listFileName.c_str(), std::ifstream::in);
    if(!listFile.is_open()) {
        cout << "\nError opening List File";
        return -1;
    }

    string imageString;
    int line_count = 0;
    while(getline(listFile, line)) {
        if(line_count == imageIdx) {
            imageString = line;
//            cout << imageString;
            fflush(stdout);
            break;
        }
        line_count++;
    }
    listFile.close();

    string keyName = queryMap[imageIdx];
    cout << keyName;

    unsigned char* refKeys;
    keypt_t* refKeysInfo;
    int numRefKeys = ReadKeyFile( keyName.c_str(), 
            &refKeys, &refKeysInfo );

    ifstream camParFile(camFilePath.c_str(), std::ifstream::in);
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

    if(matches3D_2D.size() == 0) {
        printf("\nNo matches to process, exiting");
        return 0;
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
    fflush(stdout);

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


    printf("\nPreparing arrays");
    fflush(stdout);
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

    
    vector<int> inliers_final;
    bool status = BundleRegisterImage(data, pt3, pt2, pt3_idx, pt2_idx, NULL, K, R, t, inliers_final);
    if(status) {
        printf("\nSuccessfully Registered Image %d", imageIdx);
        fflush(stdout);

        FILE* camOutFile = fopen(camOutPath.c_str(), "w");
        if(camOutFile == NULL) {
            cout << "\nError Opening Cam Out File";
            return -1;
        }

        FILE* trackOutFile = fopen(trackOutPath.c_str(),"w");
        if(trackOutFile == NULL) {
            cout << "\nError Opening Track Out File";
            return -1;
        }

        fprintf(camOutFile, "%0.8e %0.8e %0.8e\n", 
                data.m_camera.m_focal, 
                data.m_camera.m_k[0], data.m_camera.m_k[1]);
        fprintf(camOutFile, "%0.8e %0.8e %0.8e\n", 
                data.m_camera.m_R[0], data.m_camera.m_R[1], 
                data.m_camera.m_R[2]);
        fprintf(camOutFile, "%0.8e %0.8e %0.8e\n", 
                data.m_camera.m_R[3], data.m_camera.m_R[4], 
                data.m_camera.m_R[5]);
        fprintf(camOutFile, "%0.8e %0.8e %0.8e\n", 
                data.m_camera.m_R[6], data.m_camera.m_R[7], 
                data.m_camera.m_R[8]);
        fprintf(camOutFile, "%0.8e %0.8e %0.8e\n", 
                data.m_camera.m_t[0], 
                data.m_camera.m_t[1], data.m_camera.m_t[2]);


        for(int i=0; i < inliers_final.size(); i++) {
            int idx = inliers_final[i];
            pair<int,int> p = finalMatches2[idx];

            float x = refKeysInfo[p.second].nx; 
            float y = refKeysInfo[p.second].ny; 

            fprintf(trackOutFile, "%d %d %d %f %f\n", 
                    p.first, imageIdx, p.second, x, y);
        }
        fclose(camOutFile);
        fclose(trackOutFile);

    } else {
        printf("\nFailed to register image %d", imageIdx);
        fflush(stdout);
    }
    printf("\nDone with camera correction for %d", imageIdx);
    return 0;
}

#include <stdio.h>
#include <vector>
#include <map>
using namespace std;
int main(int argc, char* argv[]) {
    FILE* fp1 = fopen(argv[1], "r");
    FILE* fp2 = fopen(argv[2], "r");

    int id;
    float err;
    map<int,float> errorMap;
    while(fscanf(fp1,"%d %f\n",&id,&err) != EOF) {
        errorMap.insert(make_pair(id,err));
    }

    int locIdx = -1;
    while(fscanf(fp2,"%d\n",&locIdx) != EOF) {
        map<int,float>::iterator it = errorMap.find(locIdx);
        if(it != errorMap.end()) {
            printf("%d %f\n", (*it).first, (*it).second);
        }
    }

    fclose(fp1);
    fclose(fp2);
}

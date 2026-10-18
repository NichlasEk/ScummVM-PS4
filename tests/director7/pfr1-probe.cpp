#include <fstream>
#include <iterator>
#include <vector>
#include <cassert>
#include "bridge.h"
int main(int argc,char**argv){std::ifstream in(argv[1],std::ios::binary);std::vector<unsigned char>d((std::istreambuf_iterator<char>(in)),{});unsigned char*out=nullptr;size_t n=0;assert(!director_pfr1_to_ttf(nullptr,0,&out,&n));assert(!out&&!n);assert(director_pfr1_to_ttf(d.data(),d.size(),&out,&n));std::ofstream f(argv[2],std::ios::binary);f.write((char*)out,n);director_pfr1_free(out);for(size_t k=0;k<58;k++)assert(!director_pfr1_to_ttf(d.data(),k,&out,&n));d[0]=0;assert(!director_pfr1_to_ttf(d.data(),d.size(),&out,&n));}

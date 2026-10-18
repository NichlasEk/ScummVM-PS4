#include <fstream>
#include <iterator>
#include <iostream>
#include "libreshockwave/font/Pfr1Font.hpp"
#include "libreshockwave/font/Pfr1TtfConverter.hpp"
int main(int argc,char**argv){if(argc!=3)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>d((std::istreambuf_iterator<char>(in)),{});auto f=libreshockwave::font::Pfr1Font::parse(d);if(!f)return 3;auto t=libreshockwave::font::Pfr1TtfConverter::convert(*f,"Sand");if(t.empty())return 4;std::ofstream out(argv[2],std::ios::binary);out.write((char*)t.data(),t.size());std::cout<<f->glyphs.size()<<" glyphs, "<<t.size()<<" bytes\n";}

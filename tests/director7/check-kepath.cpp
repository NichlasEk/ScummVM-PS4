#include <cassert>
#include "engines/director/lingo/xtras/k/kepath-grid.h"
using namespace Director::KePathGrid;
int main() {
 std::vector<Cell> cells;
 for(int y=0;y<7;++y) for(int x=0;x<7;++x)
  if(x != 3 || y == 6) cells.emplace_back(x,y);
 auto path=find(cells,{0,0},{6,0});
 assert(!path.empty() && path.back()==Cell(6,0));
 Cell prev(0,0);
 bool crossedGap=false;
 for(auto p:path) {
  assert(std::find(cells.begin(),cells.end(),p)!=cells.end());
  assert(std::abs(p.first-prev.first)<=1 && std::abs(p.second-prev.second)<=1);
  if(p==Cell(3,6)) crossedGap=true;
  prev=p;
 }
 assert(crossedGap);
 cells.erase(std::find(cells.begin(),cells.end(),Cell(3,6)));
 assert(find(cells,{0,0},{6,0}).empty());
 assert(find(cells,{0,0},{0,0}).empty());
 assert(find(cells,{0,0},{99,99}).empty());
 std::vector<Cell> diagonal={{0,0},{1,1},{2,2}};
 assert(find(diagonal,{0,0},{2,2}).size()==2);
}

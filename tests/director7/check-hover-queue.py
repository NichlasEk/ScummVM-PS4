#!/usr/bin/env python3
"""Execute the actual Jönsson hover transition with reentrant input adapters."""
from pathlib import Path
import subprocess, sys, tempfile
source = (Path(sys.argv[1]) / 'engines/director/events.cpp').read_text()
start = source.index('if (isPS4Jonsson()) {', source.index('if (spriteId != _lastEnteredChannelId)'))
end = source.index('\n\t\t\t\t// Scripts may pump events', start)
body = source[start:end]
program = r'''
#include <cassert>
#include <deque>
#include <functional>
#include <string>
#include <vector>
using uint16 = unsigned short;
enum { VOIDSYM, kEventMouseEnter, kEventMouseLeave };
struct Event { int kind, id; };
struct State { std::vector<int> callstack; };
struct Lingo {
 State state; State *_state=&state; struct { int type=VOIDSYM; } _currentInputEvent;
 std::function<void(Event)> handler;
 void processEvents(std::deque<Event>& q, bool) {
  while (!q.empty()) {
   Event e=q.front(); q.pop_front(); state.callstack.push_back(1);
   handler(e); state.callstack.pop_back();
  }
 }
};
struct Movie {
 Lingo lingo; Lingo *_lingo=&lingo;
 uint16 _lastEnteredChannelId=0;
 std::deque<Event> _inputEventQueue;
 bool isPS4Jonsson() { return true; }
 void queueInputEvent(int kind,int id,int) { _inputEventQueue.push_back({kind,id}); }
 bool move(uint16 spriteId) {
  bool result=true; int pos=0;
  if (spriteId != _lastEnteredChannelId) {
   const uint16 previous=_lastEnteredChannelId;
''' + body + r'''
  }
  return result;
 }
};
int main() {
 Movie m; std::string names[4]={"", "sickan_walk_0", "vanis_walk_0", "harry_walk_0"};
 std::vector<Event> delivered;
 bool nested=false;
 m.lingo.handler=[&](Event e) {
  delivered.push_back(e);
  auto &name=names[e.id];
  if(e.kind==kEventMouseEnter) {
   assert(name.size()<2 || name.substr(name.size()-2)!="_r");
   name+="_r";
  } else {
   assert(name.substr(name.size()-2)=="_r"); name.resize(name.size()-2);
   if(!nested) { nested=true; m.move(3); }
  }
 };
 m.move(1); m.move(1); assert(delivered.size()==1);
 // Events raised while another script edits a card must wait.
 m.lingo.state.callstack.push_back(1); m.move(2);
 assert(delivered.size()==1 && m._inputEventQueue.size()==2);
 m.lingo.state.callstack.pop_back();
 m.lingo.processEvents(m._inputEventQueue,true);
 assert(delivered.size()==5);
 assert(delivered[1].kind==kEventMouseLeave && delivered[1].id==1);
 assert(delivered[2].kind==kEventMouseEnter && delivered[2].id==2);
 assert(delivered[3].kind==kEventMouseLeave && delivered[3].id==2);
 assert(delivered[4].kind==kEventMouseEnter && delivered[4].id==3);
 for(int i=0;i<100;++i) { m.move(1);m.move(2);m.move(3);m.move(0); }
 assert(names[1]=="sickan_walk_0" && names[2]=="vanis_walk_0" && names[3]=="harry_walk_0");
 // A suspended input handler also defers dispatch.
 m.lingo._currentInputEvent.type=99; auto count=delivered.size(); m.move(1);
 assert(delivered.size()==count && m._inputEventQueue.size()==1);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 path=Path(tmp); (path/'test.cpp').write_text(program)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
 subprocess.run([str(path/'test')],check=True)
print('Hover queue: busy/suspended scripts, nested transitions, duplicate moves and 100 card cycles passed')

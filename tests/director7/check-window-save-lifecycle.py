#!/usr/bin/env python3
"""Exercise actual destructor, save-list and existence branches with sanitizer-backed fixtures.

Mocks provide ownership observers, an input stack, and a save-file catalogue;
production method bodies are extracted unchanged from the prepared source.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('source', type=Path)
a = p.parse_args()
root = a.source.resolve()
window = (root / 'engines/director/window.cpp').read_text()
start = window.index('Window::~Window() {')
end = window.index('\n}\n', start) + 3
destructor = window[start:end]
builtins = (root / 'engines/director/lingo/lingo-builtins.cpp').read_text()
start = builtins.index('void LB::b_getNthFileNameInFolder(int nargs) {')
end = builtins.index('\n\t// getNthFileNameInFolder requires an absolute path', start)
listing = builtins[start:end] + '\n normalPathReached = true;\n}\n'
header = (root / 'engines/director/ps4-jonsson.h').read_text()
start = header.index('inline Common::String ps4JonssonSaveName(')
helper = header[start:header.index('\n}', start)+2]
budapi = (root / 'engines/director/lingo/xtras/b/budapi.cpp').read_text()
start = budapi.index('void BudAPIXtra::m_baFileExists(int nargs) {')
exists = budapi[start:budapi.index('\n}', start)+2]
code = r'''
#include <algorithm>
#include <cassert>
#include <cctype>
#include <fnmatch.h>
#include <string>
#include <vector>
using uint = unsigned;
namespace Common {
struct String : std::string {
 using std::string::string;
 String(const std::string &s) : std::string(s) {}
 bool hasSuffix(const String &s) const {
  return size() >= s.size() && compare(size()-s.size(),s.size(),s)==0;
 }
 size_t findLastOf(char c) const { return find_last_of(c); }
 bool hasSuffixIgnoreCase(String s) const { String t=*this;t.toLowercase();s.toLowercase();return t.hasSuffix(s); }
 bool hasPrefixIgnoreCase(String s) const { String t=*this;t.toLowercase();s.toLowercase();return t.compare(0,s.size(),s)==0; }
 void setChar(char c, uint i) { (*this)[i]=c; }
 void deleteLastChar() { pop_back(); }
 void toLowercase() { for (char &c : *this) c=std::tolower((unsigned char)c); }
};
using StringArray = std::vector<String>;
using std::sort;
}
struct Window;
struct Director;
extern Director *g_director;
struct Resource {
 Window *owner;
 explicit Resource(Window *w):owner(w) {}
 ~Resource();
};
struct WM {
 void setLockedWidget(void *) {}
 void removeWindow(Resource *r) { delete r; }
 void removeMarked() {}
};
struct Window {
 Resource *_lingoState, *_lingoPlayState, *_soundManager, *_currentMovie;
 std::vector<Resource *> _frozenLingoStates;
 Resource *_puppetTransition, *_window;
 bool _isModal = true;
 WM *_wm;
 explicit Window(WM *wm):_lingoState(new Resource(this)),
  _lingoPlayState(new Resource(this)), _soundManager(new Resource(this)),
  _currentMovie(new Resource(this)), _frozenLingoStates{new Resource(this)},
  _puppetTransition(new Resource(this)), _window(new Resource(this)), _wm(wm) {}
 ~Window();
};
struct Director {
 Window *cursor = nullptr;
 WM *_wm;
 Common::String game = "jonssonligan1";
 Window *getCursorWindow() const { return cursor; }
 void setCursorWindow(Window *w) { cursor=w; }
 const char *getGameId() const { return game.c_str(); }
} director;
Director *g_director = &director;
unsigned destroyedResources = 0;
Resource::~Resource() {
 // Destruction can re-enter drawing. The owner must already be invalidated.
 assert(g_director->getCursorWindow() != owner);
 ++destroyedResources;
}
struct Datum {
 Common::String s; int n=0;
 Datum(const Common::String &v):s(v) {}
 Datum(const char *v):s(v) {}
 Datum(int v):n(v) {}
 int asInt() const { return n; }
 Common::String asString() const { return s; }
};
struct Lingo {
 std::vector<Datum> stack;
 Datum pop() { assert(!stack.empty()); Datum d=stack.back();stack.pop_back();return d; }
 void push(Datum d) { stack.push_back(d); }
} lingo;
Lingo *g_lingo = &lingo;
struct Saves {
 Common::StringArray catalogue;
 bool exists(const Common::String &s) { return std::find(catalogue.begin(),catalogue.end(),s)!=catalogue.end(); }
 Common::StringArray listSavefiles(const Common::String &pattern) {
  Common::StringArray result;
  for (const auto &f : catalogue)
   if (!fnmatch(pattern.c_str(),f.c_str(),0)) result.push_back(f);
  return result;
 }
} saves;
struct System { Saves *getSavefileManager() { return &saves; } } systemStub;
System *g_system = &systemStub;
bool enabled = true, normalPathReached = false;
bool isPS4Jonsson() { return enabled && (director.game=="jonssonligan1" || director.game=="jonssonligan2"); }
Common::String savePrefix() { return "custom-target-"; }
Common::String findPath(Common::String s, bool, bool, bool) { normalPathReached=true;return s=="asset.dat" ? s : Common::String(); }
namespace BudAPIXtra { void m_baFileExists(int); }
namespace LB { void b_getNthFileNameInFolder(int); }
'''
code += destructor + '\n' + listing + '\n' + helper + '\n' + exists
code += r'''
Common::String list(const char *path, int index) {
 normalPathReached=false;
 assert(lingo.stack.empty());
 lingo.push(Datum(path));lingo.push(Datum(index));
 LB::b_getNthFileNameInFolder(2);
 if (normalPathReached) { assert(lingo.stack.empty());return "<normal>"; }
 assert(lingo.stack.size()==1);
 return lingo.pop().asString();
}
int fileExists(const char *path) {
 normalPathReached=false;assert(lingo.stack.empty());lingo.push(Datum(path));
 BudAPIXtra::m_baFileExists(1);assert(lingo.stack.size()==1);return lingo.pop().asInt();
}
int main() {
 WM wm;director._wm=&wm;
 Window *a=new Window(&wm), *b=new Window(&wm);
 director.cursor=a;
 delete b;assert(director.cursor==a); // unrelated window must not clear cursor
 delete a;assert(!director.cursor);
 delete new Window(&wm);assert(!director.cursor);
 assert(destroyedResources==21);
 saves.catalogue={"custom-target-Zebra.txt", "other-target-Secret.txt",
  "custom-target-Jönsson1.txt", "custom-target-pref-736372496e666f"};
 assert(list("toInstall\\Sparade spel\\",1)=="Jönsson1");
 assert(list("TOINSTALL/Sparade Spel/",2)=="Zebra");
 for (int n : {-1,0,3,999}) assert(list("Sparade spel",n).empty());
 assert(list("",1).empty());
 assert(list("data",1)=="<normal>");
 saves.catalogue={"custom-target-save_02.sav.txt", "custom-target-Zebra.txt",
  "custom-target-save_01.sav.txt", "other-target-save_00.sav.txt"};
 director.game="jonssonligan2";
 assert(list("C:\\SAVEJON\\",1)=="save_01.sav");
 assert(list("C:/savejon",2)=="save_02.sav");
 assert(list("C:/savejon",3).empty());
 director.game="jonssonligan1";
 saves.catalogue={"custom-target-Jönsson1.txt"};
 assert(fileExists("toInstall\\Sparade spel\\Jönsson1")==1 && !normalPathReached);
 assert(fileExists("Sparade spel/Jönsson1.txt")==1);
 assert(fileExists("SPARADE SPEL/custom-target-Jönsson1.txt")==1);
 assert(fileExists("Sparade spel/missing")==0 && !normalPathReached);
 assert(fileExists("other/Jönsson1")==0 && normalPathReached);
 assert(fileExists("asset.dat")==1 && normalPathReached);
 director.game="jonssonligan2";
 assert(fileExists("Sparade spel/Jönsson1")==0 && normalPathReached);
 director.game="anothergame";assert(list("Sparade spel",1)=="<normal>");
 director.game="jonssonligan1";enabled=false;
 assert(list("Sparade spel",1)=="<normal>");
 assert(fileExists("Sparade spel/Jönsson1")==0 && normalPathReached);
}
'''
with tempfile.TemporaryDirectory(prefix='director-window-save-') as d:
    d = Path(d)
    (d / 'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++11', '-g', '-O1', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', str(d / 'test.cpp'), '-o', str(d / 'test')], check=True)
    subprocess.run([str(d / 'test')], check=True)
print('PASS: cursor invalidated before teardown, unrelated cursor retained; title-specific save listing, target isolation, bounds, save existence mapping and stack balance; ASan/UBSan')

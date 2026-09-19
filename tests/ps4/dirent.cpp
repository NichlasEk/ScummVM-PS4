#include "backends/fs/posix/ps4-dirent.h"
#include <assert.h>
int main() {
 uint8_t record[512] = {};
 record[4]=12; record[7]=3; memcpy(record+8,"abc",4);
 uint16_t size; uint8_t names;
 assert(ps4DirentRecord(record,12,size,names) && size==12 && names==3);
 for (size_t length=0; length<12; ++length) assert(!ps4DirentRecord(record,length,size,names));
 record[4]=0; assert(!ps4DirentRecord(record,512,size,names));
 record[4]=255;record[5]=255; assert(!ps4DirentRecord(record,512,size,names));
 record[4]=12;record[5]=0;record[7]=255; assert(!ps4DirentRecord(record,512,size,names));
 record[7]=3;record[11]='x'; assert(!ps4DirentRecord(record,12,size,names));
 record[11]=0;record[9]=0; assert(!ps4DirentRecord(record,12,size,names));
 record[9]='/'; assert(!ps4DirentRecord(record,12,size,names));
 record[9]='b'; assert(ps4DirentRecord(record,12,size,names));
 assert(!ps4DirentRecord(nullptr,12,size,names));
}

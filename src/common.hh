//MIT License
//
//Copyright (c) 2019 The University of Texas at Austin
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.
//
//Author(s) : Esha Choukse



#ifndef __COMMON_HH__
#define __COMMON_HH__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <assert.h>

//--------------------------------------------------------------------
#define LSIZE (512)  // in bits
//#define LSIZE (1024)  // in bits

#define EXC (0.3) // Exception percent for LCP

#define _LINES_PER_PAGE ((4096/LSIZE)*8)
#define SUBPAGES 4
#define LINES_PER_SUBPAGE (_LINES_PER_PAGE/SUBPAGES)
//--------------------------------------------------------------------
using namespace std;

unsigned block_sizes[3][16] = {
	{0,0,64,64,256,256,512,512}, //0
	{0,0,176,176,352,352,512,512}, //1
	{0,0,128,128,256,256,512,512}}; //2

int page_sizes[2][8] = {{512*8, 512*8*2, 512*8*3, 512*8*4, 512*8*5, 512*8*6, 512*8*7, 512*8*8},
			{512*8, 512*8, 1024*8, 1024*8, 2048*8, 2048*8, 4096*8, 4096*8}};

typedef bool                BOOL;
typedef uint8_t             UINT8;
typedef uint16_t            UINT16;
typedef uint32_t            UINT32;
typedef uint64_t            UINT64;
typedef int8_t              INT8;
typedef int16_t             INT16;
typedef int32_t             INT32;
typedef int64_t             INT64;
typedef float               FLT32;
typedef double              FLT64;

typedef unsigned long long  CNT;
typedef unsigned            LENGTH;
typedef unsigned long long  KEY;

static const INT32 _MAX_BYTES_PER_LINE     = LSIZE/8;
static const INT32 _MAX_WORDS_PER_LINE     = LSIZE/16;
static const INT32 _MAX_DWORDS_PER_LINE    = LSIZE/32;
static const INT32 _MAX_QWORDS_PER_LINE    = LSIZE/64;
static const INT32 _MAX_FLOATS_PER_LINE    = LSIZE/32;
static const INT32 _MAX_DOUBLES_PER_LINE   = LSIZE/64;

typedef struct { UINT64 m : 52; UINT64 e : 11; UINT64 s : 1; } FLT64_P;
typedef struct { UINT32 m : 23; UINT32 e : 8;  UINT32 s : 1; } FLT32_P;

//--------------------------------------------------------------------
typedef union CACHELINE_DATA {
	UINT8   byte[_MAX_BYTES_PER_LINE];
	UINT16  word[_MAX_WORDS_PER_LINE];
	UINT32  dword[_MAX_DWORDS_PER_LINE];
	UINT64  qword[_MAX_QWORDS_PER_LINE];

	INT8    s_byte[_MAX_BYTES_PER_LINE];
	INT16   s_word[_MAX_WORDS_PER_LINE];
	INT32   s_dword[_MAX_DWORDS_PER_LINE];
	INT64   s_qword[_MAX_QWORDS_PER_LINE];

	FLT32   flt[_MAX_FLOATS_PER_LINE];
	FLT64   dbl[_MAX_DOUBLES_PER_LINE];
	FLT32_P flt_p[_MAX_FLOATS_PER_LINE];
	FLT64_P dbl_p[_MAX_DOUBLES_PER_LINE];
} CACHELINE_DATA;

typedef union BITPLANE_DATA {
	UINT32  dword[32];
} BITPLANE_DATA;
//--------------------------------------------------------------------
class Compressor {
	public:
		// constructor / destructor        
		Compressor(const string _name) : name(_name) { }
		~Compressor() {}
	public:
		// methods
		string getName() const { return name; }
		CNT getPatternCnt(INT64 pattern) { auto it = patternCounterMap.find(pattern); return (it==patternCounterMap.end()) ? 0 : it->second; }

		virtual LENGTH compressLine(CACHELINE_DATA* line, UINT64 line_addr) = 0;
		virtual void reset() {
			totalPatternCnt = 0ull;
			totalLineCnt = 0ull;
			patternCounterMap.clear();
			lengthMap.clear();
		}

	protected:
		void compressFile(FILE *fd) {
			CACHELINE_DATA line;

			reset();

			while (true) {
				if (fread(&line, _MAX_BYTES_PER_LINE, 1, fd)!=1) {
					break;
				}
				compressLine(&line, 0);
			}
		}
		virtual void countPattern(INT64 pattern) {
			totalPatternCnt++;
			auto it = patternCounterMap.find(pattern);
			if (it==patternCounterMap.end()) {
				patternCounterMap.insert(pair<INT64, CNT>(pattern, 1ull));
			} else {
				it->second++;
			}
		}
		virtual void countLineResult(LENGTH length) {
			totalLineCnt++;
			auto it = lengthMap.find(length);
			if (it==lengthMap.end()) {
				lengthMap.insert(pair<LENGTH, CNT>(length, 1ull));
			} else {
				it->second++;
			}
		}
		double getCoverage(int thresholdBitSize) {
			CNT accumLineCnt = 0ull;
			for (int i=0; i<=thresholdBitSize; i++) {
				auto it = lengthMap.find(i);
				if (it!=lengthMap.end()) {
					accumLineCnt += it->second;
				}
			}
			return accumLineCnt*1./totalLineCnt;
		}
	public:
		virtual void printSummary(FILE* fd) {
			CNT accumCnt;

			fprintf(fd, "Comp\t%s\n", name.c_str());
			// Input bench data
			fprintf(fd, "dataCnt32\t%lld\n", totalLineCnt*_MAX_DWORDS_PER_LINE);

			// Compression results
			accumCnt = 0ull;
			for (auto it = lengthMap.begin(); it != lengthMap.end(); ++it) {
				accumCnt += (it->first * it->second);
			}
			fprintf(fd, "compratio  \t%f\n", totalLineCnt*_MAX_DWORDS_PER_LINE*32./accumCnt);
			printf(" %f\n", totalLineCnt*_MAX_DWORDS_PER_LINE*32./accumCnt);
			fprintf(fd, "Comp32b    \t%f\n", accumCnt*1./totalLineCnt/_MAX_DWORDS_PER_LINE);
			fprintf(fd, "Comp8b     \t%f\n", accumCnt*1./totalLineCnt/_MAX_BYTES_PER_LINE);
			/*        fprintf(fd, "CompDist   \t");
				  accumCnt = 0ull;
				  for (int i=0; i<600; i++) {
				  auto it = lengthMap.find(i);
				  if (it!=lengthMap.end()) {
				  accumCnt += it->second;
				  }
				  if (i%8==0) {
				  fprintf(fd, "%f\t", accumCnt*1./totalLineCnt);
				  }
				  }
				  fprintf(fd, "\n");
			 */
		}

		virtual void printLCPSummary(FILE* fd, CNT accumCnt) {

			fprintf(fd, "Comp\t%s\n", name.c_str());
			// Input bench data
			fprintf(fd, "dataCnt32\t%lld\n", totalLineCnt*_MAX_DWORDS_PER_LINE);

			// Compression results
			fprintf(fd, "compratio  \t%f\n", totalLineCnt*_MAX_DWORDS_PER_LINE*32./accumCnt);
			printf(" %f\n", totalLineCnt*_MAX_DWORDS_PER_LINE*32./accumCnt);
			fprintf(fd, "Comp32b    \t%f\n", accumCnt*1./totalLineCnt/_MAX_DWORDS_PER_LINE);
			fprintf(fd, "Comp8b     \t%f\n", accumCnt*1./totalLineCnt/_MAX_BYTES_PER_LINE);
		}
		virtual void printDetails(FILE* fd, string bench_name) const {
			fprintf(fd, "Pattern frequency\n");
			// if too huge
			if (patternCounterMap.size() > (1<<16)) {
				map<INT64, CNT> patternGroupCounterMap;
				// group into 65536 groups
				int offset = 48;
				for (auto it = patternCounterMap.begin(); it != patternCounterMap.end(); ++it) {
					INT64 patternGroup = it->first>>offset;
					auto it2 = patternGroupCounterMap.find(patternGroup);
					if (it2 == patternGroupCounterMap.end()) {
						patternGroupCounterMap.insert(pair<INT64, CNT>(patternGroup, it->second));
					} else {
						it2->second += it->second;
					}
				}
				if (true) {     // sorted based on pattern
					vector<pair<INT64, CNT>>v;
					for (auto it = patternGroupCounterMap.begin(); it != patternGroupCounterMap.end(); ++it) {
						v.push_back(pair<INT64, CNT>(it->first, it->second));
					}
					// sorting
					sort(v.begin(), v.end());

					for (auto it = v.begin(); it != v.end(); ++it) {
						fprintf(fd, "%16lx\t%f\t%f\n", (UINT64) it->first, it->second*1./totalPatternCnt, -log2(it->second*1./totalPatternCnt));
					}
				} else {        // sorted based on frequency
					vector<pair<CNT, INT64>>v;
					for (auto it = patternGroupCounterMap.begin(); it != patternGroupCounterMap.end(); ++it) {
						v.push_back(pair<INT64, CNT>(it->second, it->first));
					}
					// sorting
					sort(v.begin(), v.end());

					for (auto it = v.begin(); it != v.end(); ++it) {
						fprintf(fd, "%16lx\t%f\t%f\n", (UINT64) it->second, it->first*1./totalPatternCnt, -log2(it->first*1./totalPatternCnt));
					}
				}
			} else {
				if (true) {     // sorted based on pattern
					vector<pair<INT64, CNT>>v;
					for (auto it = patternCounterMap.begin(); it != patternCounterMap.end(); ++it) {
						v.push_back(pair<INT64, CNT>(it->first, it->second));
					}
					// sorting
					sort(v.begin(), v.end());

					for (auto it = v.begin(); it != v.end(); ++it) {
						fprintf(fd, "%16lx\t%f\t%f\n", (UINT64) it->first, it->second*1./totalPatternCnt, -log2(it->second*1./totalPatternCnt));
					}
				} else {        // sorted based on frequency
					vector<pair<CNT, INT64>>v;
					for (auto it = patternCounterMap.begin(); it != patternCounterMap.end(); ++it) {
						v.push_back(pair<INT64, CNT>(it->second, it->first));
					}
					// sorting
					sort(v.begin(), v.end());

					for (auto it = v.begin(); it != v.end(); ++it) {
						fprintf(fd, "%16lx\t%f\t%f\n", (UINT64) it->second, it->first*1./totalPatternCnt, -log2(it->first*1./totalPatternCnt));
					}
				}
			}

			fprintf(fd, "Compressed line size\n");
			for (auto it=lengthMap.begin(); it!=lengthMap.end(); it++) {
				fprintf(fd, "%02d\t%f\t%f\n", it->first, it->second*1./ totalLineCnt, -log2(it->second*1./totalLineCnt));
			}
		}
	protected:
		string name;

		CNT totalPatternCnt;
		CNT totalLineCnt;
		map<INT64, CNT> patternCounterMap;
		map<LENGTH, CNT> lengthMap;
};

bool sign_extended(UINT64 value, UINT8 bit_size);
bool zero_extended(UINT64 value, UINT8 bit_size);

//--------------------------------------------------------------------
#endif /* __COMMON_HH__ */

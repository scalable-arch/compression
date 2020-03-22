// MIT License
//
// Copyright (c) 2020 SungKyunKwan University
// Copyright (c) 2019 The University of Texas at Austin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Author(s) : Jungrae Kim
//           : Esha Choukse


#ifndef __CPACK_COMPRESSOR_HH__
#define __CPACK_COMPRESSOR_HH__

#include "common.hh"

class CPackCompressor: public Compressor {
public:
    CPackCompressor(): Compressor("C-Pack64") {}

    // external interface
public:
    LENGTH compressLine(CACHELINE_DATA *line, UINT64 line_addr) {
        LENGTH blkLength = 0;

        int wrPtr = 0;
        for (int i=0; i<16; i++) {
            dictionary[i] = 0;
        }

        for (UINT32 i=0; i<_MAX_DWORDS_PER_LINE; i++) {
            // code 00: zzzz
            if (line->dword[i]==0) {
                countPattern(0);
                blkLength+=2;
            }
            else {
                bool matchedFull = false;
                bool matched3B = false;
                bool matched2B = false;
                for (int j=0; j<16; j++) {
                    if (line->dword[i]==dictionary[j]) {
                        matchedFull = true;
                    }
                    if ((line->dword[i]&0xFFFFFF00)==(dictionary[j]&0xFFFFFF00)) {
                        matched3B = true;
                    }
                    if ((line->dword[i]&0xFFFF0000)==(dictionary[j]&0xFFFF0000)) {
                        matched2B = true;
                    }
                }

                // code 10: mmmm
                if (matchedFull) {
                    countPattern(2);
                    blkLength+=6;
                }
                // code 1101: zzzx  -> 1101+8-bit
                else if ((line->byte[i*4+3]==0)&&(line->byte[i*4+2]==0)&&(line->byte[i*4+1]==0)) {
                    countPattern(13);
                    blkLength+=12;
                }
                // code 1110: mmmx  -> 1110+4-bit+8-bit
                else if (matched3B) {
                    countPattern(14);
                    blkLength+=16;
                }
                // code 1100: mmxx  -> 1100+4-bit+8-bitx2
                else if (matched2B) {
                    countPattern(12);
                    blkLength+=24;
                }
                // code: 01: xxxx -> 34 bit
                else {
                    countPattern(1);
                    blkLength+=34;
                    dictionary[wrPtr] = line->dword[i];
                    wrPtr = (wrPtr+1)%16;
                }
            }
        }

        countLineResult(blkLength);

        return blkLength;
    }

protected:
    INT32 dictionary[16];
};

#endif /* __CPACK_COMPRESSOR_HH__ */

// MIT License
// 
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
// Author(s) : Esha Choukse

#ifndef __FPCOMPRESSOR_HH__
#define __FPCOMPRESSOR_HH__

#include "common.hh"

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
class FPCompressorDW: public Compressor {     // up to 256 patterns
public:
    // constructor / destructor
    FPCompressorDW() : Compressor("FPC-DW") {}

    // external interface
public:
    LENGTH compressLine(CACHELINE_DATA *line, UINT64 line_addr) {
        unsigned zeroRun = 0;
        unsigned blkLength = 0;

        for (unsigned i=0; i<_MAX_DWORDS_PER_LINE; i++) {
            // 000
            if (line->dword[i]==0) { // zero dword
                zeroRun++;
                if (zeroRun==8) {
                    blkLength += (3+3);
                    countPattern(0);
                    zeroRun = 0;
                }
            } else {
                if (zeroRun!=0) {   // previously zero-run
                    blkLength += (3+3);
                    countPattern(0);
                    zeroRun = 0;
                }

                if (sign_extended(line->dword[i], 4)) {         // 1
                    countPattern(1);
                    blkLength += (4+3);
                } else if (sign_extended(line->dword[i], 8)) {  // 2
                    countPattern(2);
                    blkLength += (8+3);
                } else if (   (line->byte[i*4]==line->byte[i*4+1])
                           && (line->byte[i*4]==line->byte[i*4+2])
                           && (line->byte[i*4]==line->byte[i*4+3])) {   // 6
                    countPattern(6);
                    blkLength += (8+3);
                } else if (sign_extended(line->dword[i], 16)) { // 3
                    countPattern(3);
                    blkLength += (16+3);
                } else if (line->word[i*2]==0) {                // 4
                    countPattern(4);
                    blkLength += (16+3);
                } else if (sign_extended(line->word[i*2+1], 8) && sign_extended(line->word[i*2], 8)) {  // 5
                    countPattern(5);
                    blkLength += (16+3);
                } else {                                        // 7
                    countPattern(7);
                    blkLength += (32+3);
                }
            }
        }

        if (zeroRun!=0) {
            blkLength += (3+3);
            countPattern(0);
        }

        countLineResult(blkLength);

        return blkLength;
    }
};
#endif /* __FPCOMPRESSOR_HH__ */

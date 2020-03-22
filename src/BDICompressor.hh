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

#ifndef __BDI_COMPRESSOR_HH__
#define __BDI_COMPRESSOR_HH__

#include "common.hh"
bool sign_extended(UINT64 value, UINT8 bit_size) {
    UINT64 max = (1ULL << (bit_size-1)) - 1;    // bit_size: 4 -> ...00000111
    UINT64 min = ~max;                          // bit_size: 4 -> ...11111000
    return (value <= max) | (value >= min);
}

bool zero_extended(UINT64 value, UINT8 bit_size) {
    UINT64 max = (1ULL << (bit_size)) - 1;      // bit_size: 4 -> ...00001111
    return (value <= max);
}

class BDCompressorQW : public Compressor {
public:
    BDCompressorQW(): Compressor("BD-QW") {}
    BDCompressorQW(string name): Compressor(name) {}

    // external interface
public:
    LENGTH compressLine(CACHELINE_DATA *line, UINT64 line_addr) {
        LENGTH blkLength;

        // 0000: zeros
        // : one byte for 32-byte line / one byte for 64-byte line / one byte for 128-byte line
        if (zero(line)) {
            countPattern(0);
            blkLength = 8*1+4;
        }
        // 0001: repeated values
        // : 8 bytes for 32-byte line / 8 bytes for 64-byte line / 8 byte for 128-byte line
        else if (rep_qword(line)) {
            countPattern(1);
            blkLength = 8*8+4;
        }
        // 0010: base8 + delta1
        // 12 bytes for 32-byte line / 16-bytes for 64-byte line; / 24 bytes for 128-byte line
        else if (BD_qword(line, 1)) {
            countPattern(2);
            blkLength = 8*24+4;
            //updateLength(8*(8 + _MAX_QWORDS_PER_LINE*1) + 4, minLength, 0x2, minPattern);
        }
        // 0101: base4 + delta1
        // 12 bytes for 32-byte line / 20 bytes for 64-byte line / 36 bytes for 128-byte line
        else if (BD_dword(line, 1)) {
            countPattern(5);
            blkLength = 8*36+4;
            //updateLength(8*(4 + _MAX_DWORDS_PER_LINE*1) + 4, minLength, 0x5, minPattern);
        }
        // 0011: base8 + delta2
        // 16 bytes for 32-byte line / 24-bytes for 64-byte line / 40-bytes for 128-byte line
        else if (BD_qword(line, 2)) {
            countPattern(3);
            blkLength = 8*40+4;
            //updateLength(8*(8 + _MAX_QWORDS_PER_LINE*2) + 4, minLength, 0x3, minPattern);
        }
        // 0111: base2 + delta1
        // 18 bytes for 32-byte line / 34 bytes for 64-byte line / 66 bytes for 128-byte line
        else if (BD_word(line, 1)) {
            countPattern(7);
            blkLength = 8*66+4;
            //updateLength(8*(2 + _MAX_WORDS_PER_LINE*1) + 4, minLength, 0x7, minPattern);
        }
        // 0110: base4 + delta2
        // 20 bytes for 32-byte line / 36 bytes for 64-byte line / 68 bytes for 128-byte line
        else if (BD_dword(line, 2)) {
            countPattern(6);
            blkLength = 8*68+4;
            //updateLength(8*(4 + _MAX_DWORDS_PER_LINE*2) + 4, minLength, 0x5, minPattern);
        }
        // 0100: base8 + delta4
        // 24 bytes for 32-byte line / 40 bytes for 64-byte line / 72 bytes for 128-byte line
        else if (BD_qword(line, 4)) {
            countPattern(4);
            blkLength = 8*72+4;
            //updateLength(8*(8 + _MAX_QWORDS_PER_LINE*4) + 4, minLength, 0x4, minPattern);
        }
        // default (no compression)
        // : original data + 4-bit prefix
        else {
            countPattern(0xf);
            blkLength = _MAX_BYTES_PER_LINE*8 + 4;
        }

        countLineResult(blkLength);

        return blkLength;
    }

protected:
    bool zero(CACHELINE_DATA *line) {
        for (UINT32 i=0; i<_MAX_QWORDS_PER_LINE; i++) {
            if (line->qword[i]!=0) {
                return false;
            }
        }
        return true;
    }
    bool rep_qword(CACHELINE_DATA *line) {
        for (unsigned i=1; i<_MAX_QWORDS_PER_LINE; i++) {
            if (line->qword[i]!=line->qword[0]) {
                return false;
            }
        }
        return true;
    }
    bool rep_dword(CACHELINE_DATA *line) {
        for (unsigned i=1; i<_MAX_DWORDS_PER_LINE; i++) {
            if (line->dword[i]!=line->dword[0]) {
                return false;
            }
        }
        return true;
    }
    bool BD_qword(CACHELINE_DATA *line, UINT32 delta_byte_size) {
        for (unsigned i=1; i<_MAX_QWORDS_PER_LINE; i++) {
            INT64 delta = line->qword[i] - line->qword[0];
            if (!sign_extended(delta, delta_byte_size*8)) {
                return false;
            }
        }
        return true;
    }
    bool BD_dword(CACHELINE_DATA *line, UINT32 delta_byte_size) {
        for (unsigned i=1; i<_MAX_DWORDS_PER_LINE; i++) {
            INT64 delta = line->dword[i] - line->dword[0];
            if (!sign_extended(delta, delta_byte_size*8)) {
                return false;
            }
        }
        return true;
    }
    bool BD_word(CACHELINE_DATA *line, UINT32 delta_byte_size) {
        for (UINT32 i=1; i<_MAX_WORDS_PER_LINE; i++) {
            INT64 delta = line->word[i] - line->word[0];
            if (!sign_extended(delta, delta_byte_size*8)) {
                return false;
            }
        }
        return true;
    }
};

class BDICompressorQW : public BDCompressorQW {
public:
    BDICompressorQW(): BDCompressorQW("BDI-QW") {}

    // external interface
public:
    LENGTH compressLine(CACHELINE_DATA *line, UINT64 line_addr) {
        LENGTH blkLength;

        // 0000: zeros
        // : one byte for 32-byte line / one byte for 64-byte line / one byte for 128-byte line
        if (zero(line)) {
            countPattern(0);
            blkLength = 8*1+4;
        }
        // 0001: repeated 64-bit
        // : 8 bytes for 32-byte line / 8 bytes for 64-byte line / 8 byte for 128-byte line
        else if (rep_qword(line)) {
            countPattern(1);
            blkLength = 8*8+4;
        }
        // 0010: base8 + delta1
        // 12.5 bytes for 32-byte line / 17-bytes for 64-byte line; / 26 bytes for 128-byte line
        else if (BDI_qword(line, 1)) {
            countPattern(2);
            blkLength = 8*24+16+4;
            //updateLength(8*(8 + _MAX_QWORDS_PER_LINE*1)+_MAX_QWORDS_PER_LINE+4, minLength, 0x2, minPattern);
        }
        // 0101: base4 + delta1
        // 13 bytes for 32-byte line / 22 bytes for 64-byte line / 40 bytes for 128-byte line
        else if (BDI_dword(line, 1)) {
            countPattern(5);
            blkLength = 8*36+32+4;
            //updateLength(8*(4 + _MAX_DWORDS_PER_LINE*1)+_MAX_DWORDS_PER_LINE+4, minLength, 0x5, minPattern);
        }
        // 0011: base8 + delta2
        // 16.5 bytes for 32-byte line / 25-bytes for 64-byte line / 42-bytes for 128-byte line
        else if (BDI_qword(line, 2)) {
            countPattern(3);
            blkLength = 8*40+16+4;
            //updateLength(8*(8 + _MAX_QWORDS_PER_LINE*2)+_MAX_QWORDS_PER_LINE+4, minLength, 0x3, minPattern);
        }
        // 0110: base4 + delta2
        // 21 bytes for 32-byte line / 38 bytes for 64-byte line / 72 bytes for 128-byte line
        else if (BDI_dword(line, 2)) {
            countPattern(6);
            blkLength = 8*68+32+4;
            //updateLength(8*(4 + _MAX_DWORDS_PER_LINE*2)+_MAX_DWORDS_PER_LINE+4, minLength, 0x6, minPattern);
        }
        // 0111: base2 + delta1
        // 20 bytes for 32-byte line / 38 bytes for 64-byte line / 74 bytes for 128-byte line
        else if (BDI_word(line, 1)) {
            countPattern(7);
            blkLength = 8*66+64+4;
            //updateLength(8*(2 + _MAX_WORDS_PER_LINE*1)+_MAX_WORDS_PER_LINE+4, minLength, 0x7, minPattern);
        }
        // 0100: base8 + delta4
        // 24.5 bytes for 32-byte line / 41 bytes for 64-byte line / 74 bytes for 128-byte line
        else if (BDI_qword(line, 4)) {
            countPattern(4);
            blkLength = 8*72+16+4;
            //updateLength(8*(8 + _MAX_QWORDS_PER_LINE*4)+_MAX_QWORDS_PER_LINE+4, minLength, 0x4, minPattern);
        }
        // default (no compression)
        // : original data + 4-bit prefix
        else {
            countPattern(0xf);
            blkLength = _MAX_BYTES_PER_LINE*8 + 4;
        }

        countLineResult(blkLength);

        return blkLength;
    }

protected:
    bool BDI_qword(CACHELINE_DATA *line, UINT32 delta_byte_size) {
        UINT64 base;
        bool isBaseFound = false;
        for (unsigned i=0; i<_MAX_QWORDS_PER_LINE; i++) {
            if (sign_extended(line->qword[i], delta_byte_size*8)) {
                // stored as immediate
            } else {
                if (isBaseFound==false) {
                    // stored as base
                    base = line->qword[i];
                    isBaseFound = true;
                } else {
                    // stored as delta
                    INT64 delta = line->qword[i] - base;
                    if (!sign_extended(delta, delta_byte_size*8)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
    bool BDI_dword(CACHELINE_DATA *line, UINT32 delta_byte_size) {
        UINT64 base;
        bool isBaseFound = false;
        for (unsigned i=0; i<_MAX_DWORDS_PER_LINE; i++) {
            if (sign_extended(line->dword[i], delta_byte_size*8)) {
                // stored as immediate
            } else {
                if (isBaseFound==false) {
                    // stored as base
                    base = line->dword[i];
                    isBaseFound = true;
                } else {
                    // stored as delta
                    INT64 delta = line->dword[i] - base;
                    if (!sign_extended(delta, delta_byte_size*8)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
    bool BDI_word(CACHELINE_DATA *line, UINT32 delta_byte_size) {
        UINT64 base;
        bool isBaseFound = false;
        for (unsigned i=0; i<_MAX_WORDS_PER_LINE; i++) {
            if (sign_extended(line->word[i], delta_byte_size*8)) {
                // stored as immediate
            } else {
                if (isBaseFound==false) {
                    // stored as base
                    base = line->word[i];
                    isBaseFound = true;
                } else {
                    // stored as delta
                    INT64 delta = line->word[i] - base;
                    if (!sign_extended(delta, delta_byte_size*8)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
};

#endif /* __BDI_COMPRESSOR_HH__ */

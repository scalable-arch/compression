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


#ifndef __BP_COMPRESSOR_HH__
#define __BP_COMPRESSOR_HH__

#include "common.hh"
//------------------------------------------------------------------------------
bool sign_extended(UINT64 value, UINT8 bit_size) {
    UINT64 max = (1ULL << (bit_size-1)) - 1;    // bit_size: 4 -> ...00000111
    UINT64 min = ~max;                          // bit_size: 4 -> ...11111000
    return (value <= max) | (value >= min);
}

bool zero_extended(UINT64 value, UINT8 bit_size) {
    UINT64 max = (1ULL << (bit_size)) - 1;      // bit_size: 4 -> ...00001111
    return (value <= max);
}
class BPCompressor64 : public ECompressor {
public:
    BPCompressor64(const string name) : ECompressor(name) {}
    unsigned compressLine(CACHELINE_DATA* line, UINT64 line_addr) {
        INT64 deltas[15];
        bool delta_signs[15];
        for (int i=1; i<_MAX_QWORDS_PER_LINE; i++) {
            deltas[i-1] = ((INT64) line->s_qword[i]) - ((INT64) line->s_qword[i-1]);
            if (line->s_qword[i]>= line->s_qword[i-1]) {
                delta_signs[i-1] = 0;
            } else {
                delta_signs[i-1] = 1;
            }
        }

        INT16 buf = 0;
        INT16 prevDBP;
        INT16 DBP[65];
        INT16 DBX[65];
        for (int i=15; i>=0; i--) {
            buf <<= 1;
            buf |= (delta_signs[i]&1);
        }
        DBP[64] = DBX[64] = prevDBP = buf;
        for (int j=63; j>=0; j--) {
            buf = 0;
            for (int i=30; i>=0; i--) {
                buf <<= 1;
                buf |= ((deltas[i]>>j)&1);
            }
            DBP[j] = buf;
            DBX[j] = buf^prevDBP;
            prevDBP = buf;
        }

        // first 32-bit word in original form
        unsigned blkLength = encodeFirst(line->s_qword[0]);
        blkLength += encodeDeltas(DBP, DBX);

        countLineResult(blkLength);
        return blkLength;
    }

    virtual unsigned encodeFirst(INT64 sym) {
        if (sym==0) {
            countPattern(256);
            return 3;
        } else if (sign_extended(sym, 4)) {
            countPattern(257);
            return (4+4);
        } else if (sign_extended(sym, 8)) {
            countPattern(258);
            return (4+8);
        } else if (sign_extended(sym, 16)) {
            countPattern(259);
            return (3+16);
        } else if (sign_extended(sym, 32)) {
            countPattern(260);
            return (3+32);
        //} else if ((sym&0x0000FFFF)==0) {
        //    countPattern(260);
        //    return (3+16);
        //} else if (sign_extended(sym&0xFFFF, 8) && sign_extended(sym>>16, 8)) {
        //    countPattern(261);
        //    return (3+16);
        //} else if (   (sym>>24==(sym&0xFF))
        //           && (sym>>16==(sym&0xFF))
        //           && (sym>> 8==(sym&0xFF)) ) {
        //    countPattern(262);
        //    return (3+8);
        } else {
            countPattern(263);
            return (1+64);
        }

        //if ((sym&0xFFFFFF00)==0) {   // {2'b10,8bit}
        //    return 10;
        //} else if ((sym&0xFFFF0000)==0) {    // {2'b11,16-bit}
        //    return 18;
        //} else {    // {1'b0, 32-bit}
        //    return 33;
        //}
    }
    virtual unsigned encodeDeltas(INT16* DBP, INT16* DBX) {
        static const unsigned ZRL_CODE_SIZE[66] = {0, 3, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9};
        // 1        -> uncompressed
        // 01       -> Z-RLE: 2~65
        // 001      -> Z-RLE: 1
        // 00000    -> single 1
        // 00001    -> consecutive two 1Â´s
        // 00010    -> zero DBP
        // 00011    -> All 1Â´s

        unsigned length = 0;
        unsigned run_length = 0;
        bool firstNZDBX = false;
        bool secondNZDBX = false;
        for (int i=64; i>=0; i--) {
            if (DBX[i]==0) {
                run_length++;
            }
            else {
                if (run_length>0) {
                    countPattern(run_length-1);
                    assert(run_length!=33);
                    length += ZRL_CODE_SIZE[run_length];
                }
                run_length = 0;

                if (DBP[i]==0) {
                    length += 5;
                    countPattern(33);
                } else if (DBX[i]==0x7fff) {
                    length += 5;
                    countPattern(34);
                } else {
                    int oneCnt = 0;
                    for (int j=0; j<16; j++) {
                        if ((DBX[i]>>j)&1) {
                            oneCnt++;
                        }
                    }
                    unsigned two_distance = 0;
                    int firstPos = -1;
                    if (oneCnt<=2) {
                        for (int j=0; j<16; j++) {
                            if ((DBX[i]>>j)&1) {
                                if (firstPos==-1) {
                                    firstPos = j;
                                } else {
                                    two_distance = j - firstPos;
                                }
                            }
                        }
                    }
                    if (oneCnt==1) {
                        length += 9;
                        countPattern(36+firstPos);
                    } else if ((oneCnt==2) && (two_distance==1)) {
                        length += 9;
                        countPattern(68+firstPos);
                    } else {
                        //if (i==32) {
                        //    printf("@%02d %08x %08x\n", i, DBX[i], DBP[i]);
                        //} else {
                        //    printf("@%02d %08x %08x %08x\n", i, DBX[i], DBP[i], DBP[i+1]);
                        //}
                        length += 16;
                        countPattern(35);
                    }
                }
            }
        }
        if (run_length>0) {
            length += ZRL_CODE_SIZE[run_length];
            countPattern(run_length-1);
            assert(run_length<=65);
        }
        return length;
    }
};
class BPSCompressorDW : public ECompressor {
public:
    BPSCompressorDW(const string name, int diff, int bp, int code, int fragblocks)
    : ECompressor(name), diff_mode(diff), bp_mode(bp), code_mode(code), frag_mode(fragblocks){}
    ~BPSCompressorDW() {}
public:
    void reset() {
        ECompressor::reset();

        prev_zero = true;
        prev_data = 0;
        prev_delta = 0;
        prev_line = {};

        run_length = 0;
    }

    CACHELINE_DATA* transform(CACHELINE_DATA* line, CACHELINE_DATA &buffer) {
        if (diff_mode==0) {              // raw
            return line;
        } else if (diff_mode==1) {       // delta
            for (int i=0; i<_MAX_DWORDS_PER_LINE; i++) {
                buffer.dword[i] = (line->dword[i] - prev_data);
                prev_data = line->dword[i];
            }
        } else if (diff_mode==2) {       // XOR
            for (int i=0; i<_MAX_DWORDS_PER_LINE; i++) {
                buffer.dword[i] = (line->dword[i] ^ prev_data);
                prev_data = line->dword[i];
            }
        } else if (diff_mode==3) {      // block delta
            for (int i=0; i<_MAX_DWORDS_PER_LINE; i++) {
                buffer.dword[i] = (prev_line.dword[i] - line->dword[i]);
            }
            prev_line = *line;
        } else if (diff_mode==4) {      // delta-delta
            for (unsigned i=0; i<_MAX_DWORDS_PER_LINE; i++) {
                INT32 delta = line->dword[i] - prev_data;
                buffer.dword[i] = (prev_delta - delta);
                prev_data = line->dword[i];
                prev_delta = delta;
            }
        } else if (diff_mode==5) {
            buffer.dword[0] = line->dword[0];
            for (int i=1; i<_MAX_DWORDS_PER_LINE; i++) {
                buffer.dword[i] = (line->dword[i] - line->dword[i-1]);
            }
        } else if (diff_mode==6) {
            buffer.dword[0] = line->dword[0];
            buffer.dword[1] = line->dword[1] - line->dword[0];
            for (int i=2; i<_MAX_DWORDS_PER_LINE; i++) {
                buffer.dword[i] = (line->dword[i] - line->dword[i-2]);
            }
        }
        return &buffer;
    }
    unsigned compressLine(CACHELINE_DATA* line, UINT64 line_addr) {
        CACHELINE_DATA diff_buffer;
        CACHELINE_DATA *diff_result = transform(line, diff_buffer);

        // BP mode
//TODO: These sizes need to be changed for a smaller cache line size
        CACHELINE_DATA bp_buffer = {};
        CACHELINE_DATA dbp_buffer = {};
        CACHELINE_DATA dbx_buffer = {};
        CACHELINE_DATA dbx2_buffer = {};
        CACHELINE_DATA *bp_result = NULL;

        if (bp_mode==0) {           // no BP
            bp_result = diff_result;
        } else if (bp_mode==4) {
            for (int j=31; j>=0; j--) {
                INT32 bufBP = 0;
                INT32 bufDBP = 0;
                INT32 bufDBX = 0;
                INT32 bufDBX2 = 0;
                for (int i=_MAX_DWORDS_PER_LINE-1; i>=1; i--) {
                    bufBP   <<= 1;
                    bufDBP  <<= 1;
                    bufDBX  <<= 1;
                    bufDBX2 <<= 1;
                    bufBP   |= ((line->dword[i]>>j)&1);
                    bufDBP  |= ((diff_result->dword[i]>>j)&1);
                    if (j==31) {
                        bufDBX  |= ((diff_result->dword[i]>>j)&1);
                        bufDBX2 |= ((diff_result->dword[i]>>j)&1);
                    } else {
                        bufDBX  |= (((diff_result->dword[i]>>j)^(diff_result->dword[i]>>(j+1)))&1);
                        bufDBX2 |= (((diff_result->dword[i]>>j)^(diff_result->dword[i]>>(31)))&1);
                    }
                }
                bp_buffer.dword[j]   = bufBP;
                dbp_buffer.dword[j]  = bufDBP;
                dbx_buffer.dword[j]  = bufDBX;
                dbx2_buffer.dword[j] = bufDBX2;
            }
            bp_result = &dbx_buffer;
        } else {                    // BP / BPX
            for (int j=31; j>=0; j--) {
                INT32 bufBP = 0;
                INT32 bufDBP = 0;
                INT32 bufDBX = 0;
                INT32 bufDBX2 = 0;
                for (int i=_MAX_DWORDS_PER_LINE-1; i>=0; i--) {
                    bufBP   <<= 1;
                    bufDBP  <<= 1;
                    bufDBX  <<= 1;
                    bufDBX2 <<= 1;
                    bufBP   |= ((line->dword[i]>>j)&1);
                    bufDBP  |= ((diff_result->dword[i]>>j)&1);
                    if (j==31) {
                        bufDBX  |= ((diff_result->dword[i]>>j)&1);
                        bufDBX2 |= ((diff_result->dword[i]>>j)&1);
                    } else {
                        bufDBX  |= (((diff_result->dword[i]>>j)^(diff_result->dword[i]>>(j+1)))&1);
                        bufDBX2 |= (((diff_result->dword[i]>>j)^(diff_result->dword[i]>>(31)))&1);
                    }
                }
                bp_buffer.dword[j]   = bufBP;
                dbp_buffer.dword[j]  = bufDBP;
                dbx_buffer.dword[j]  = bufDBX;
                dbx2_buffer.dword[j] = bufDBX2;
            }
            if (bp_mode==1) {
                bp_result = &bp_buffer;
            } else if (bp_mode==2) {
                bp_result = &dbx_buffer;
            } else if (bp_mode==3) {
                bp_result = &dbx2_buffer;
            } else {
                assert(0);
            }
        }
        unsigned blkLength = 0;
        if (code_mode==10) {
            blkLength = encode_paper(&dbx_buffer, &dbp_buffer, line);
        }
        else if (code_mode==11) {
            if (((diff_buffer.dword[0])&0xFFFFFF00)==0) {   // {2'b10,8bit}
                blkLength += 10;
            } else if (((diff_buffer.dword[0])&0xFFFF0000)==0) {    // {2'b11,16-bit}
                blkLength += 18;
            } else {    // {1'b0, 32-bit}
                blkLength += 33;
            }
            blkLength += encode_paper2(&dbx_buffer, &dbp_buffer);
        }

//Fragmentation as per cache block size

        if(frag_mode<4) {
            for (int i=1; i<sizeof(block_sizes[frag_mode]); i++) {
                if(blkLength > block_sizes[frag_mode][i]) {
                    blkLength = block_sizes[frag_mode][i-1];
                    break;
                }
                if(i== sizeof(block_sizes[frag_mode])-1)
                    blkLength = block_sizes[frag_mode][i];
            }
        }

        if (blkLength > LSIZE)
            blkLength = LSIZE;
        countLineResult(blkLength);

        return blkLength;
    }

    unsigned encode_paper(CACHELINE_DATA *dbx, CACHELINE_DATA *dbp, CACHELINE_DATA *line) {
        //static const unsigned ZRL_CODE_SIZE[33] = {0, 4, 8, 6, 8, 11, 7, 7, 9, 10, 9, 8, 9, 9, 10, 10, 10, 11, 9, 9, 10, 5, 8, 9, 10, 11, 11, 6, 9, 7, 10, 8, 10};

        static const unsigned ZRL_CODE_SIZE[33] = {0, 4, 6, 7, 8, 9, 6, 10, 12, 12, 8, 8, 9, 10, 9, 11, 11, 9, 9, 9, 10, 11, 10, 9, 7, 8, 8, 5, 7, 11, 10, 11, 8};

        unsigned length = 0;
        run_length = 0;
        for (int i=_MAX_DWORDS_PER_LINE-1; i>=0; i--) {
            if (dbx->dword[i]==0) {
                run_length++;
            }
            else {
                if (run_length>0) {
                    countPattern(run_length-1);
                    assert(run_length!=32);
                    length += ZRL_CODE_SIZE[run_length] + 1; //ESHA
                }
                run_length = 0;
//ESHA
//MIKE: 2/14/19, Removing Line Bug
                if(false && line->dword[i]==0) {
                    length += 1;
                    countPattern(512);
                }
                else {
                    length += 1;
                    if (dbx->dword[i]==1) {
                        length += 3;
                        countPattern(32);
                    } else if (dbp->dword[i]==0) {
                        length += 4;
                        countPattern(33);
                    } else if (dbx->dword[i]==0xffffffff) {
                        length += 7;
                        countPattern(34);
                    } else if (dbx->dword[i]==0xfffffffe) {
                        length += 9;
                        countPattern(35);
                    } else {
                        int oneCnt = 0;
                        for (int j=0; j<32; j++) {
                            if ((dbx->dword[i]>>j)&1) {
                                oneCnt++;
                            }
                        }
                        unsigned two_distance = 0;
                        int firstPos = -1;
                        if (oneCnt<=2) {
                            for (int j=0; j<32; j++) {
                                if ((dbx->dword[i]>>j)&1) {
                                    if (firstPos==-1) {
                                        firstPos = j;
                                    } else {
                                        two_distance = j - firstPos;
                                    }
                                }
                            }
                        }
                        if (oneCnt==1) {
                            length += 10;
                            countPattern(64+firstPos);
                        } else if ((oneCnt==2) && (firstPos==0)) {
                            length += 9;
                            countPattern(96+firstPos);
                        } else if ((oneCnt==2) && (two_distance==1)) {
                            length += 11;
                            countPattern(128+firstPos);
                        } else {
                            length += 33;
                            countPattern(36);
                        }
                    }
                }
            }
        }
        if (run_length>0) {
            length += ZRL_CODE_SIZE[run_length];
            countPattern(run_length-1);
            assert(run_length<=32);
        }
        return length;
    }
    unsigned encode_paper2(CACHELINE_DATA *dbx, CACHELINE_DATA *dbp) {
        //static const unsigned ZRL_CODE_SIZE[33] = {0, 4, 8, 6, 8, 11, 7, 7, 9, 10, 9, 8, 9, 9, 10, 10, 10, 11, 9, 9, 10, 5, 8, 9, 10, 11, 11, 6, 9, 7, 10, 8, 10};
        //static const unsigned ZRL_CODE_SIZE[33] = {0, 4, 6, 7, 8, 9, 6, 10, 12, 12, 8, 8, 9, 10, 9, 11, 11, 9, 9, 9, 10, 11, 10, 9, 7, 8, 8, 5, 7, 11, 10, 11, 8};
        static const unsigned ZRL_CODE_SIZE[33] = {0, 4, 6, 7, 9, 8, 5, 9, 11, 11, 8, 8, 10, 10, 9, 11, 12, 9, 8, 8, 9, 10, 10, 10, 10, 8, 7, 5, 6, 9, 8, 6, 6};

        unsigned length = 0;
        run_length = 0;
        for (int i=_MAX_DWORDS_PER_LINE-1; i>=0; i--) {
            if (dbx->dword[i]==0) {
                run_length++;
            }
            else {
                //printf("@%02d\t%08x\n", i, dbx->dword[i]);
                if (run_length>0) {
                    countPattern(run_length-1);
                    assert(run_length!=32);
                    length += ZRL_CODE_SIZE[run_length];
                }
                run_length = 0;

                if (dbp->dword[i]==0) {
                    length += 4;
                    countPattern(33);
                } else if (dbx->dword[i]==0x7fffffff) {
                    length += 5;
                    countPattern(34);
                } else {
                    int oneCnt = 0;
                    for (int j=0; j<32; j++) {
                        if ((dbx->dword[i]>>j)&1) {
                            oneCnt++;
                        }
                    }
                    unsigned two_distance = 0;
                    int firstPos = -1;
                    if (oneCnt<=2) {
                        for (int j=0; j<32; j++) {
                            if ((dbx->dword[i]>>j)&1) {
                                if (firstPos==-1) {
                                    firstPos = j;
                                } else {
                                    two_distance = j - firstPos;
                                }
                            }
                        }
                    }
                    if (oneCnt==1) {
                        length += 8;
                        countPattern(64+firstPos);
                    } else if ((oneCnt==2) && (two_distance==1)) {
                        length += 10;
                        countPattern(128+firstPos);
                    } else {
                        length += 32;
                        countPattern(36);
                    }
                }
            }
        }
        if (run_length>0) {
            length += ZRL_CODE_SIZE[run_length];
            countPattern(run_length-1);
            assert(run_length<=32);
        }
        return length;
    }
protected:
    // zero run length counters
    int diff_mode;
    int bp_mode;
    int code_mode;
    int frag_mode;

    INT32 prev_data;
    INT32 prev_delta;
    CACHELINE_DATA prev_line;
    int run_length;
    bool prev_zero;
};

class BPCompressor : public ECompressor {
public:
    BPCompressor(const string name) : ECompressor(name) {}
    unsigned compressLine(CACHELINE_DATA* line, UINT64 line_addr) {
        INT64 deltas[31];
        for (int i=1; i<_MAX_DWORDS_PER_LINE; i++) {
            deltas[i-1] = ((INT64) line->s_dword[i]) - ((INT64) line->s_dword[i-1]);
        }

        INT32 prevDBP;
        INT32 DBP[33];
        INT32 DBX[33];
        for (int j=63; j>=0; j--) {
            INT32 buf = 0;
            for (int i=30; i>=0; i--) {
                buf <<= 1;
                buf |= ((deltas[i]>>j)&1);
            }
            if (j==63) {
                DBP[32] = buf;
                DBX[32] = buf;
                prevDBP = buf;
            } else if (j<32) {
                DBP[j] = buf;
                DBX[j] = buf^prevDBP;
                prevDBP = buf;
            } else {
                assert(buf==prevDBP);
                prevDBP = buf;
            }
        }

        // first 32-bit word in original form
        unsigned blkLength = encodeFirst(line->dword[0]);
        blkLength += encodeDeltas(DBP, DBX);

        //if (blkLength > 768) {
        //    for (int i=0; i<_MAX_DWORDS_PER_LINE; i++) {
        //        printf("@Orig%02d %08x\n", i, line->dword[i]);
        //    }
        //    for (int i=1; i<_MAX_DWORDS_PER_LINE; i++) {
        //        printf("@Delta%02d %09ld\n", i, deltas[i-1]);
        //    }
        //    for (int i=1; i<_MAX_WORDS_PER_LINE; i++) {
        //        printf("@delta%02d %06d\n", i, ((INT32) line->s_word[i]) - ((INT32) line->s_word[i-1]));
        //    }
        //    for (int i=32; i>=0; i--) {
        //        printf("@DBP%02d ", i);
        //        for (int j=30; j>=0; j--) {
        //            printf("%0d", (DBP[i]>>j)&1);
        //            //if ((j%4==0)&&(j!=0)) {
        //            //    printf("_");
        //            //}
        //        }
        //        printf("\n");
        //    }
        //    for (int i=32; i>=0; i--) {
        //        printf("@DBX%02d ", i);
        //        for (int j=30; j>=0; j--) {
        //            printf("%0d", (DBX[i]>>j)&1);
        //            //if ((j%4==0)&&(j!=0)) {
        //            //    printf("_");
        //            //}
        //        }
        //        printf("\n");
        //    }
        //}
        countLineResult(blkLength);
        return blkLength;
    }

    virtual unsigned encodeFirst(INT32 sym) {
        if (sym==0) {
            countPattern(256);
            return 3;
        } else if (sign_extended(sym, 4)) {
            countPattern(257);
            return (3+4);
        } else if (sign_extended(sym, 8)) {
            countPattern(258);
            return (3+8);
        } else if (sign_extended(sym, 16)) {
            countPattern(259);
            return (3+16);
        //} else if ((sym&0x0000FFFF)==0) {
        //    countPattern(260);
        //    return (3+16);
        //} else if (sign_extended(sym&0xFFFF, 8) && sign_extended(sym>>16, 8)) {
        //    countPattern(261);
        //    return (3+16);
        //} else if (   (sym>>24==(sym&0xFF))
        //           && (sym>>16==(sym&0xFF))
        //           && (sym>> 8==(sym&0xFF)) ) {
        //    countPattern(262);
        //    return (3+8);
        } else {
            countPattern(263);
            return (1+32);
        }

        //if ((sym&0xFFFFFF00)==0) {   // {2'b10,8bit}
        //    return 10;
        //} else if ((sym&0xFFFF0000)==0) {    // {2'b11,16-bit}
        //    return 18;
        //} else {    // {1'b0, 32-bit}
        //    return 33;
        //}
    }
    virtual unsigned encodeDeltas(INT32* DBP, INT32* DBX) {
        //static const unsigned ZRL_CODE_SIZE[33] = {0, 4, 8, 6, 8, 11, 7, 7, 9, 10, 9, 8, 9, 9, 10, 10, 10, 11, 9, 9, 10, 5, 8, 9, 10, 11, 11, 6, 9, 7, 10, 8, 10};
        //static const unsigned ZRL_CODE_SIZE[33] = {0, 4, 6, 7, 8, 9, 6, 10, 12, 12, 8, 8, 9, 10, 9, 11, 11, 9, 9, 9, 10, 11, 10, 9, 7, 8, 8, 5, 7, 11, 10, 11, 8};
        //static const unsigned ZRL_CODE_SIZE[34] = {0, 4, 6, 7, 9, 8, 5, 9, 11, 11, 8, 8, 10, 10, 9, 11, 12, 9, 8, 8, 9, 10, 10, 10, 10, 8, 7, 5, 6, 9, 8, 6, 6, 6};
        //static const unsigned ZRL_CODE_SIZE[34] = {0, 4, 6, 8, 9, 8, 5, 7, 11, 11, 8, 9, 8, 10, 10, 10, 12, 12, 9, 8, 9, 9, 10, 10, 11, 9, 8, 7, 5, 6, 10, 8, 6, 6};
        //static const unsigned singleOneSize = 8;
        //static const unsigned consecutiveDoubleOneSize = 10;
        //static const unsigned allOneSize = 5;
        //static const unsigned zeroDBPSize = 4;
        static const unsigned ZRL_CODE_SIZE[34] = {0, 3, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
        static const unsigned singleOneSize = 10;
        static const unsigned consecutiveDoubleOneSize = 10;
        static const unsigned allOneSize = 5;
        static const unsigned zeroDBPSize = 5;
        // 1        -> uncompressed
        // 01       -> Z-RLE: 2~33
        // 001      -> Z-RLE: 1
        // 00000    -> single 1
        // 00001    -> consecutive two 1Â´s
        // 00010    -> zero DBP
        // 00011    -> All 1Â´s

        unsigned length = 0;
        unsigned run_length = 0;
        bool firstNZDBX = false;
        bool secondNZDBX = false;
        for (int i=32; i>=0; i--) {
            if (DBX[i]==0) {
                run_length++;
            }
            else {
                if (run_length>0) {
                    countPattern(run_length-1);
                    assert(run_length!=33);
                    length += ZRL_CODE_SIZE[run_length];
                }
                run_length = 0;

                if (DBP[i]==0) {
                    length += zeroDBPSize;
                    countPattern(34);
                } else if (DBX[i]==0x7fffffff) {
                    length += allOneSize;
                    countPattern(35);
                } else {
                    int oneCnt = 0;
                    for (int j=0; j<32; j++) {
                        if ((DBX[i]>>j)&1) {
                            oneCnt++;
                        }
                    }
                    unsigned two_distance = 0;
                    int firstPos = -1;
                    if (oneCnt<=2) {
                        for (int j=0; j<32; j++) {
                            if ((DBX[i]>>j)&1) {
                                if (firstPos==-1) {
                                    firstPos = j;
                                } else {
                                    two_distance = j - firstPos;
                                }
                            }
                        }
                    }
                    if (oneCnt==1) {
                        length += singleOneSize;
                        countPattern(37+firstPos);
                    } else if ((oneCnt==2) && (two_distance==1)) {
                        length += consecutiveDoubleOneSize;
                        countPattern(69+firstPos);
                    } else {
                        //if (oneCnt<8) {
                        //    if (i==32) {
                        //        printf("@%02d %08x %08x\n", i, DBX[i], DBP[i]);
                        //    } else {
                        //        printf("@%02d %08x %08x %08x\n", i, DBX[i], DBP[i], DBP[i+1]);
                        //    }
                        //}
                        length += 32;
                        countPattern(36);
                    }
                }
            }
        }
        if (run_length>0) {
            length += ZRL_CODE_SIZE[run_length];
            countPattern(run_length-1);
            assert(run_length<=33);
        }
        return length;
    }
};

class BPSCompressor64 : public ECompressor {
    public:
        BPSCompressor64(const string name, int diff, int bp, int code, int fragblocks)
            : ECompressor(name), diff_mode(diff), bp_mode(bp), code_mode(code), frag_mode(fragblocks){}
        ~BPSCompressor64() {}
    public:
        void reset() {
            ECompressor::reset();

            prev_zero = true;
            prev_data = 0;
            prev_delta = 0;
            prev_line = {};

            run_length = 0;
            run_length_orig = 0;
        }

        CACHELINE_DATA* transform(CACHELINE_DATA* line, CACHELINE_DATA &buffer) {
            if (diff_mode==2) {       // XOR
                for (int i=0; i<_MAX_DWORDS_PER_LINE; i++) {
                    buffer.dword[i] = (line->dword[i] ^ prev_data);
                    prev_data = line->dword[i];
                }
            }
            return &buffer;
        }
        unsigned compressLine(CACHELINE_DATA* line, UINT64 line_addr) {
            CACHELINE_DATA diff_buffer;
            CACHELINE_DATA *diff_result = transform(line, diff_buffer);

            // BP mode
            //TODO: These sizes need to be changed for a smaller cache line size
            CACHELINE_DATA bp_buffer = {};
            CACHELINE_DATA dbp_buffer = {};
            CACHELINE_DATA dbx_buffer = {};
            CACHELINE_DATA dbx2_buffer = {};
            CACHELINE_DATA *bp_result = NULL;

            if (bp_mode==4) {
                for (int j=31; j>=0; j--) {
                    INT32 bufDBP = 0;
                    INT32 bufDBX = 0;
                    for (int i=_MAX_DWORDS_PER_LINE-1; i>=1; i--) {
                        bufDBP  <<= 1;
                        bufDBX  <<= 1;
                        bufDBP  |= ((diff_result->dword[i]>>j)&1);
                        if (j==31) {
                            bufDBX  |= ((diff_result->dword[i]>>j)&1);
                        } else {
                            bufDBX  |= (((diff_result->dword[i]>>j)^(diff_result->dword[i]>>(j+1)))&1);
                        }
                    }
                    dbp_buffer.word[j]  = bufDBP;
                    dbx_buffer.word[j]  = bufDBX;
                }
                bp_result = &dbx_buffer;
            }
            unsigned blkLength = 0;
            if (code_mode==10) {
                blkLength = encode_paper(&dbx_buffer, &dbp_buffer, line);
            }
            countLineResult(blkLength);

            return blkLength;
        }

        unsigned encode_paper(CACHELINE_DATA *dbx, CACHELINE_DATA *dbp, CACHELINE_DATA *line) {
            //static const unsigned ZRL_CODE_SIZE[33] = {0, 4, 8, 6, 8, 11, 7, 7, 9, 10, 9, 8, 9, 9, 10, 10, 10, 11, 9, 9, 10, 5, 8, 9, 10, 11, 11, 6, 9, 7, 10, 8, 10};

            //static const unsigned ZRL_CODE_SIZE[33] = {0, 4, 6, 7, 8, 9, 6, 10, 12, 12, 8, 8, 9, 10, 9, 11, 11, 9, 9, 9, 10, 11, 10, 9, 7, 8, 8, 5, 7, 11, 10, 11, 8};

            static const unsigned ZRL_CODE_SIZE[17] = {0, 4, 6, 7, 8, 7, 6, 8, 8, 8, 8, 9, 9, 9, 9, 7, 5 };
            //	static const unsigned ZRL_CODE_SIZE[17] = {0, 3, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };
            unsigned length = 0;
            run_length = 0;
            run_length_orig = 0;
            for (int i=_MAX_DWORDS_PER_LINE-1; i>=0; i--) {

                if (dbx->dword[i]==0) {
                    run_length++;
                }
                else if((run_length==0) && (line->dword[i]==0)){
                    run_length_orig++;
                }
                else {
                    if ((run_length>0) || (run_length_orig>0)) {
                        int run_len = run_length+run_length_orig;
                        countPattern(run_len-1);
                        length += ZRL_CODE_SIZE[run_len] + 1; //ESHA
                    }
                    run_length = 0;
                    run_length_orig = 0;
                    //ESHA
                    if(line->dword[i]==0) {
                        run_length_orig++;
                    }
                    else {
                        if (dbp->dword[i]==0) {
                            length += 5;
                            countPattern(33);
                        } else if (dbx->dword[i]==0xffffffff) {
                            length += 5;
                            countPattern(34);
                        } else {
                            int oneCnt = 0;
                            for (int j=0; j<32; j++) {
                                if ((dbx->dword[i]>>j)&1) {
                                    oneCnt++;
                                }
                            }
                            unsigned two_distance = 0;
                            int firstPos = -1;
                            if (oneCnt<=2) {
                                for (int j=0; j<32; j++) {
                                    if ((dbx->dword[i]>>j)&1) {
                                        if (firstPos==-1) {
                                            firstPos = j;
                                        } else {
                                            two_distance = j - firstPos;
                                        }
                                    }
                                }
                            }
                            if (oneCnt==1) {
                                length += 10;
                                countPattern(64+firstPos);
                            } else if ((oneCnt==2) && (two_distance==1)) {
                                length += 10;
                                countPattern(128+firstPos);
                            } else {
                                length += 32;
                                countPattern(36);
                            }
                        }
                    }
                }

            }
            if ((run_length>0)||(run_length_orig>0)) {
                int run_len = run_length+run_length_orig;
                length += ZRL_CODE_SIZE[run_len]+1;
                countPattern(run_len-1);
            }
            if(run_length==16 || run_length_orig==16){
                    length = 0;	
            }
            //	cout << " " << length;
            return length;
        }
    protected:
        // zero run length counters
        int diff_mode;
        int bp_mode;
        int code_mode;
        int frag_mode;

        INT32 prev_data;
        INT32 prev_delta;
        CACHELINE_DATA prev_line;
        int run_length, run_length_orig;
        bool prev_zero;
};

#endif /* __BP_COMPRESSOR_HH__ */


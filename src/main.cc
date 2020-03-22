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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <map>
#include <list>

#include "common.hh"
#include "BPCompressor.hh"
#include "BDICompressor.hh"
#include "CPackCompressor.hh"
#include "FPCompressor.hh"

#include <sys/stat.h>
#define PAGE_SIZE 4096
#define LINE_PER_PAGE (PAGE_SIZE*8)/LSIZE

//usage:./vsc 1 cactusADM/Comppt_dump/memory/user/*
//1 : block_frag type
int main(int argc, char **argv)
{
    assert(argc>1);

    int i;
    // compressors
    list<Compressor *> comps;
    comps.push_back(new BPSCompressor64("BPC64_5", 2, 4, 10, 2));
    for (auto it = comps.cbegin(); it != comps.cend(); ++it) {
        //Per compressor outer loop
        CNT total_block_cnt = 0ull;

        int bsizeindex = 0;

        (*it)->reset();
        CNT accumCnt[3][2] = {{0ull}};
        CNT totalUncomp = 0ull;
        CNT totalOverflow=0ull;
        CNT psize=4096;
        int block_frag = (int)atoi(argv[1]);
        //cout << block_frag << endl;
        int page_frag = 0;

        // phase 1:
        for (int arg_idx = 2; arg_idx < argc; arg_idx++) {
            char bench[256];
            int i;
            int pagenum=0;
            int start_pos = 0, end_pos = strlen(argv[arg_idx])-1;
            for (i=strlen(argv[arg_idx])-1;i>=0; i--) {
                if (argv[arg_idx][i]=='/') {
                    start_pos = i+1;
                    break;
                }
            }
            for (i=0; i<=end_pos - start_pos; i++) {
                bench[i] = argv[arg_idx][start_pos+i];
            }
            bench[i] = '\0';
            struct stat st;
            stat(argv[arg_idx], &st);
            //CNT psize = st.st_size;
            FILE *fd = fopen(argv[arg_idx], "rb");
            CACHELINE_DATA line;
            CNT benchaccumCnt[3][2] = {{0ull}};
            UINT64 line_addr;
            int pageno = 0;
            unsigned size[LINE_PER_PAGE];


            while (fread(&line, LSIZE/8, 1, fd)==1) {
                total_block_cnt++;
                if (total_block_cnt%1000000==0) {
                    //printf("Processing %lld\n", total_block_cnt);
                }
                int lineno = (total_block_cnt-1) % (LINE_PER_PAGE);
                size[lineno] = (*it)->compressLine(&line, line_addr);

                if(lineno==LINE_PER_PAGE-1) {
                    pageno++;
                    int min_page=PAGE_SIZE*8; //Uncompressed page size
                    int totallen=0;
                    for(int lineid=0; lineid<LINE_PER_PAGE; lineid++) {
                        for (unsigned i=0; i<8; i++) {
                            if(size[lineid] <= block_sizes[block_frag][i]) {
                                totallen+=block_sizes[block_frag][i];
                                size[lineid] = block_sizes[block_frag][i];
                                break;
                            }
                            if(i== 7){
                                totallen+=block_sizes[block_frag][i];
                                size[lineid] = block_sizes[block_frag][i];
                            }
                        }
                        //cout << " " <<size[lineid];
                    }
                    if(totallen<min_page)
                        min_page=totallen;	
                    if(min_page!=0){
                        //THIS STEP IS FOR PAGE PACKING
                        int page_pack=1;
                        if (page_pack){
                            for(int i=0; i<8; i++) {
                                if(min_page <= page_sizes[page_frag][i]){	
                                    min_page = page_sizes[page_frag][i];
                                    break;
                                }
                            }
                        }
                    }

                    accumCnt[block_frag][page_frag]+=min_page;
                    benchaccumCnt[block_frag][page_frag]+=min_page;
                    pagenum++;
                    totalUncomp+=psize;
                }
            }

//            printf("%s %lld %lld %.2f\n", bench, psize, benchaccumCnt[block_frag][page_frag]/8, (float)(psize*8)/(float)benchaccumCnt[block_frag][page_frag]);
            fclose(fd);
        }
        CNT totalPages=totalUncomp/psize;
        printf("%s_%d_%d Total Bytes %lld Comp_Ratio: %.2f \n", (*it)->getName().c_str(), block_frag, page_frag, totalUncomp, (float)(totalUncomp*8)/(float)accumCnt[block_frag][page_frag]);
    }
}

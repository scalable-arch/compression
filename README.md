# MIT License
# 
# Copyright (c) 2020 SungKyunKwan University
# Copyright (c) 2019 The University of Texas at Austin
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 
# Author(s) : Jungrae Kim
#           : Esha Choukse

 Bit-Plane Compression
 Please cite https://ieeexplore.ieee.org/document/7551404 or https://dl.acm.org/citation.cfm?id=3001172 upon usage.
 Make: make
 Usage : ./vsc 1 <filenames of the binary memory snapshots/files --- Can be multiple>
 The 1 in the commandline chooses the block_frag as present in common.hh
 Page-level packing is switched on by default, search "PAGE PACKING" in main.cc for disabling

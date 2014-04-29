OBJECTS = PackedArray.o SuffixArrayFuns.o STAR.o Parameters.o InOutStreams.o SequenceFuns.o Genome.o Transcript.o Stats.o \
        ReadAlign.o ReadAlign_storeAligns.o ReadAlign_stitchPieces.o ReadAlign_multMapSelect.o ReadAlign_mapOneRead.o readLoad.o \
	ReadAlignChunk.o ReadAlignChunk_processChunks.o ReadAlignChunk_mapChunk.o \
	OutSJ.o outputSJ.o blocksOverlap.o ThreadControl.o sysRemoveDir.o \
        ReadAlign_maxMappableLength2strands.o binarySearch2.o\
	ReadAlign_outputAlignments.o ReadAlign_outputTranscriptBAM.o \
	ReadAlign_outputTranscriptSAM.o ReadAlign_outputTranscriptSJ.o ReadAlign_outputTranscriptCIGARp.o \
        ReadAlign_createExtendWindowsWithAlign.o ReadAlign_assignAlignToWindow.o ReadAlign_oneRead.o \
	ReadAlign_stitchWindowSeeds.o BAMfunctions.o \
        stitchWindowAligns.o extendAlign.o stitchAlignToTranscript.o alignSmithWaterman.o genomeGenerate.o \
	TimeFunctions.o ErrorWarning.o loadGTF.o streamFuns.o stringSubstituteAll.o \
        bgzf.o
SOURCES=$(wildcard *.cpp)
LDDIRS  :=/data/gingeras/user/dobin/Software/ZLIB/zlib-1.2.8_installed/lib/
LDFLAGS := -pthread -lz
LDFLAGS_static := -static -static-libgcc -pthread -L$(LDDIRS) -lz
LDFLAGS_GDB := -pthread -lz
SVNDEF := -D'SVN_VERSION_COMPILED="STAR_2.3.1z1_r399"'
COMPTIMEPLACE := -D'COMPILATION_TIME_PLACE="$(shell echo `date` $(HOSTNAME):`pwd`)"'
#OPTIMFLAGS=-fforce-addr -funsafe-loop-optimizations -ftree-loop-linear -ftree-vectorize 
#OPTIMFLAGS1=-funroll-loops  -fprefetch-loop-arrays
#OPTIMFLAGS=-fprofile-generate
#OPTIMFLAGS=-fprofile-use
#OPTIMFLAGS=-D_GLIBCXX_PARALLEL
CCFLAGS_MAIN := -std=c++0x -O3 -Wall -Wextra -fopenmp $(SVNDEF) $(COMPTIMEPLACE) $(OPTIMFLAGS) $(OPTIMFLAGS1)
CCFLAGS_GDB := -std=c++0x -g -O0 -Wall -Wextra -fopenmp $(SVNDEF) $(COMPTIMEPLACE) 
CC :=g++
GCC:=gcc
#CC :=/data/gingeras/user/dobin/Software/GCC/4.7.0/install/bin/g++

%.o : %.cpp
	$(CC) -c $(CCFLAGS) $<

all: STAR

clean :
	rm -f *.o STAR Depend.list

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),STARforMac)
ifneq ($(MAKECMDGOALS),STARforMacGDB)
Depend.list: $(SOURCES) parametersDefault.xxd
	/bin/rm -f ./Depend.list
	$(CC) $(CCFLAGS_MAIN) -MM $^ >> Depend.list
include Depend.list
endif
endif
endif

parametersDefault.xxd: parametersDefault
	xxd -i parametersDefault > parametersDefault.xxd

bgzf.o : samtools/bgzf.c
	$(GCC) -c -O3 -Wall -Wextra -I samtools/ samtools/bgzf.c

STAR : CCFLAGS=$(CCFLAGS_MAIN)
STAR : Depend.list parametersDefault.xxd $(OBJECTS)
	$(CC) -o STAR $(CCFLAGS) $(OBJECTS) $(LDFLAGS)

STARstatic : CCFLAGS=$(CCFLAGS_MAIN)
STARstatic : Depend.list parametersDefault.xxd $(OBJECTS)
	$(CC) -o STARstatic $(OBJECTS) $(CCFLAGS) $(LDFLAGS_static)

STARlong : CCFLAGS=-D'COMPILE_FOR_LONG_READS' $(CCFLAGS_MAIN)
STARlong : Depend.list parametersDefault.xxd $(OBJECTS)
	$(CC) -o STAR $(CCFLAGS) $(LDFLAGS) $(OBJECTS)

STARlongStatic : CCFLAGS=-D'COMPILE_FOR_LONG_READS' $(CCFLAGS_MAIN)
STARlongStatic : Depend.list parametersDefault.xxd $(OBJECTS)
	$(CC) -o STARstatic $(CCFLAGS) $(LDFLAGS_static) $(OBJECTS)


STARforMac : CCFLAGS=-D'COMPILE_FOR_MAC' -I ./Mac_Include/ $(CCFLAGS_MAIN)
STARforMac : parametersDefault.xxd $(OBJECTS)
	$(CC) -o STAR $(CCFLAGS) $(LDFLAGS) $(OBJECTS)

STARforMacGDB : CCFLAGS=-D'COMPILE_FOR_MAC' -I ./Mac_Include/ $(CCFLAGS_GDB)
STARforMacGDB : parametersDefault.xxd $(OBJECTS)
	$(CC) -o STAR $(CCFLAGS_GDB) $(LDFLAGS_GDB) $(OBJECTS)

gdb : CCFLAGS= $(CCFLAGS_GDB)
gdb : Depend.list parametersDefault.xxd $(OBJECTS)
	$(CC) -o STAR $(CCFLAGS_GDB) $(LDFLAGS_GDB) $(OBJECTS)

gdb-long : CCFLAGS= -D'COMPILE_FOR_LONG_READS' $(CCFLAGS_GDB)
gdb-long : Depend.list parametersDefault.xxd $(OBJECTS)
	$(CC) -o STAR $(CCFLAGS_GDB) $(LDFLAGS_GDB) $(OBJECTS)


prof : CCFLAGS=-pg $(SVNDEF) $(COMPTIMEPLACE)
prof : $(OBJECTS)
	$(CC) -pg -o STAR $(CCFLAGS) $(LDFLAGS) $(OBJECTS)

debug : CCFLAGS=-O3 -DDEBUG $(SVNDEF) $(COMPTIMEPLACE)
debug : $(OBJECTS)
	$(CC) -c $(CCFLAGS) Genome.cpp
	$(CC) -o STARdebug $(CCFLAGS) $(LDFLAGS) $(OBJECTS)

SAtxt : CCFLAGS=-D'genenomeGenerate_SA_textOutput' $(CCFLAGS_MAIN)
SAtxt : Depend.list parametersDefault.xxd $(OBJECTS)
	$(CC) -o STAR $(CCFLAGS) $(LDFLAGS) $(OBJECTS)

localChains : CCFLAGS=-D'OUTPUT_localChains' $(CCFLAGS_MAIN)
localChains : Depend.list parametersDefault.xxd $(OBJECTS)
	$(CC) -o STAR $(CCFLAGS) $(LDFLAGS) $(OBJECTS)



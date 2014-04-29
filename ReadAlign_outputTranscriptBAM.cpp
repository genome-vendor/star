#include "ReadAlign.h"
#include "SequenceFuns.h"
#include "ErrorWarning.h"
#include "IncludeDefine.h"
#include <type_traits> // C++0x

// calculate bin given an alignment covering [beg,end) (zero-based, half-close-half-open)
int reg2bin(int beg, int end)
{
    --end;
    if (beg>>14 == end>>14) return ((1<<15)-1)/7 + (beg>>14);
    if (beg>>17 == end>>17) return ((1<<12)-1)/7 + (beg>>17);
    if (beg>>20 == end>>20) return ((1<<9)-1)/7 + (beg>>20);
    if (beg>>23 == end>>23) return ((1<<6)-1)/7 + (beg>>23);
    if (beg>>26 == end>>26) return ((1<<3)-1)/7 + (beg>>26);
    return 0;
};

int bamAttrArrayWrite(int32 attr, const char* tagName, char* attrArray ) {
    attrArray[0]=tagName[0];attrArray[1]=tagName[1];
    attrArray[2]='i';
    *( (int32*) (attrArray+3))=attr;
    return 3+sizeof(int32);
};
int bamAttrArrayWrite(char attr, const char* tagName, char* attrArray ) {
    attrArray[0]=tagName[0];attrArray[1]=tagName[1];   
    attrArray[2]='A';
    attrArray[3]=attr;
    return 3+sizeof(char);
};
int bamAttrArrayWrite(string &attr, const char* tagName, char* attrArray ) {
    attrArray[0]=tagName[0];attrArray[1]=tagName[1];    
    attrArray[2]='Z';
    memcpy(attrArray+3,attr.c_str(),attr.size()+1);//copy string data including \0
    return 3+attr.size()+1;
};
int bamAttrArrayWrite(vector<char> &attr, const char* tagName, char* attrArray ) {
    attrArray[0]=tagName[0];attrArray[1]=tagName[1];    
    attrArray[2]='B';
    attrArray[3]='c';
    *( (int32*) (attrArray+4))=attr.size();
    memcpy(attrArray+4+sizeof(int32),attr.data(),attr.size());//copy array data
    return 4+sizeof(int32)+attr.size();
};
int bamAttrArrayWrite(vector<int32> &attr, const char* tagName, char* attrArray ) {
    attrArray[0]=tagName[0];attrArray[1]=tagName[1];    
    attrArray[2]='B';
    attrArray[3]='i';
    *( (int32*) (attrArray+4))=attr.size();
    memcpy(attrArray+4+sizeof(int32),attr.data(),sizeof(int32)*attr.size());//copy array data
    return 4+sizeof(int32)+sizeof(int32)*attr.size();
};

template <typename intType>
int bamAttrArrayWriteInt(intType x, const char* tagName, char* attrArray, Parameters *P) {//adapted from samtools
    attrArray[0]=tagName[0];attrArray[1]=tagName[1];    
    #define ATTR_RECORD_INT(_intChar,_intType,_intValue) attrArray[2] = _intChar; *(_intType*)(attrArray+3) = (_intType) _intValue; return 3+sizeof(_intType)
    if (x < 0) {
        if (x >= -127) {
            ATTR_RECORD_INT('c',int8_t,x);
        } else if (x >= -32767) {
            ATTR_RECORD_INT('s',int16_t,x);
        } else {
            ATTR_RECORD_INT('i',int32_t,x);
            if (!(x>=-2147483647)) {
                ostringstream errOut;
                errOut <<"EXITING because of FATAL BUG: integer out of range for BAM conversion: "<< x <<"\n";
                errOut <<"SOLUTION: contact Alex Dobin at dobin@cshl.edu\n";
                exitWithError(errOut.str(), std::cerr, P->inOut->logMain, EXIT_CODE_BUG, *P);   
            };
        };
    } else {
        if (x <= 255) {
            ATTR_RECORD_INT('C',uint8_t,x);
        } else if (x <= 65535) {
            ATTR_RECORD_INT('S',uint16_t,x);
        } else {
            ATTR_RECORD_INT('I',uint32_t,x);
            if (!(x<=4294967295)) {
                ostringstream errOut;
                errOut <<"EXITING because of FATAL BUG: integer out of range for BAM conversion: "<< x <<"\n";
                errOut <<"SOLUTION: contact Alex Dobin at dobin@cshl.edu\n";
                exitWithError(errOut.str(), std::cerr, P->inOut->logMain, EXIT_CODE_BUG, *P);   
            };
        };
    };   
};
        



uint ReadAlign::outputTranscriptBAM(Transcript const &trOut, uint nTrOut, uint iTrOut, uint mateChr, uint mateStart, char mateStrand, int unmapType, bool *mateMapped, char *outBAM) {    
    
    if (P->outSAMmode=="None") return 0; //no SAM/BAM output
    
    uint32 recSize=0; //record size - total for both mates
    uint32 recSizePrev=0; //record size for the 1st mate
    

    //for SAM output need to split mates
    uint iExMate=0; //last exon of the first mate

    uint samFLAG=0;
    uint leftMate=0; //the mate (0 or 1) which is on the left    

    bool flagPaired = P->readNmates==2;   
    
    uint nMates=1;    
    if (unmapType<0) {//unmapped reads: SAM
        for (iExMate=0;iExMate<trOut.nExons-1;iExMate++) {
            if (trOut.canonSJ[iExMate]==-3){
                nMates=2;
                break;
            };
        };
    } else {
        nMates=0;
    };

    for (uint imate=0;imate < (unmapType<0 ? nMates:P->readNmates);imate++) {
            
        uint iEx1=0;
        uint iEx2=0;
        uint Mate=0;        
        uint Str=0;
        uint32 packedCIGAR[BAM_CIGAR_MaxSize];
        uint32 nCIGAR=0; //number of CIGAR operations
        int MAPQ=0;
        uint32 attrN=0;
        char attrOutArray[BAM_ATTR_MaxSize];

        if (unmapType>=0) {//this mate is unmapped
            if (mateMapped!=NULL && mateMapped[imate]) continue; //this mate was mapped, do not record it aws unmapped
            samFLAG=0x4;
            if (P->readNmates==2) {//paired read
                samFLAG+=0x1 + (imate==0 ? 0x40 : 0x80);
                if (mateMapped[1-imate]) {//mate mapped
                    if (trBest->Str!=1-imate) samFLAG+=0x20;//mate strand reverted
                } else {//mate unmapped
                    samFLAG+=0x8;
                };
            };
            
            Mate=imate;
            Str=Mate;
            
            attrN=0;
            attrN+=bamAttrArrayWriteInt(0,"NH",attrOutArray+attrN,P);
            attrN+=bamAttrArrayWriteInt(0,"HI",attrOutArray+attrN,P);
            attrN+=bamAttrArrayWriteInt(trBest->maxScore,"AS",attrOutArray+attrN,P);
            attrN+=bamAttrArrayWriteInt(trBest->nMM,"nM",attrOutArray+attrN,P);
            attrN+=bamAttrArrayWrite((to_string((uint) unmapType)).at(0), "uT",attrOutArray+attrN); //cast to uint is only necessary for old compilers
            attrN+=bamAttrArrayWrite(P->outSAMattrRG,"RG",attrOutArray+attrN);                        

        } else {            
            
            if (flagPaired) {//paired reads
                samFLAG=0x0001;
                if (iExMate==trOut.nExons-1) {//single mate
                    if (mateChr>P->nChrReal) samFLAG+=0x0008; //not mapped as pair
                } else {//properly paired
                    samFLAG+=0x0002; //mapped as pair
                };                                 
            } else {//single end
                samFLAG=0;
            };

            iEx1 = (imate==0 ? 0 : iExMate+1);
            iEx2 = (imate==0 ? iExMate : trOut.nExons-1);
            Mate=trOut.exons[iEx1][EX_iFrag];        
            Str= trOut.Str;//note that Strand = the mate on the left

            if (Mate==0) {
                samFLAG += Str*0x10;
                if (nMates==2) samFLAG += (1-Str)*0x20;
            } else {//second mate strand need to be reverted
                samFLAG += (1-Str)*0x10;
                if (nMates==2) samFLAG += Str*0x20;            
            };        

            if (flagPaired) {
                leftMate=Str;
                samFLAG += (Mate==0 ? 0x0040 : 0x0080);
                if (flagPaired && nMates==1 && mateStrand==1) samFLAG +=0x20;//revert strand using inout value of mateStrand (e.g. for chimeric aligns)
            };        

            //not primary align?
            if (!trOut.primaryFlag) samFLAG +=0x100;

            uint trimL;
            if (Str==0 && Mate==0) {
                trimL=clip5pNtotal[Mate];
            } else if (Str==0 && Mate==1) {
                trimL=clip3pNtotal[Mate];
            } else if (Str==1 && Mate==0) {
                trimL=clip3pNtotal[Mate];
            } else {
                trimL=clip5pNtotal[Mate];
            };

            nCIGAR=0; //number of CIGAR operations

            uint trimL1 = trimL + trOut.exons[iEx1][EX_R] - (trOut.exons[iEx1][EX_R]<readLength[leftMate] ? 0 : readLength[leftMate]+1);
            if (trimL1>0) {
                packedCIGAR[nCIGAR++]=trimL1<<BAM_CIGAR_OperationShift | BAM_CIGAR_S; 
            };                      

            vector<int32> SJintron;
            vector<char> SJmotif;

            for (uint ii=iEx1;ii<=iEx2;ii++) {
                if (ii>iEx1) {//record gaps
                    uint gapG=trOut.exons[ii][EX_G]-(trOut.exons[ii-1][EX_G]+trOut.exons[ii-1][EX_L]);
                    uint gapR=trOut.exons[ii][EX_R]-trOut.exons[ii-1][EX_R]-trOut.exons[ii-1][EX_L];
                    //it's possible to have a D or N and I at the same time
                    if (gapR>0){
                          
                        packedCIGAR[nCIGAR++]=gapR<<BAM_CIGAR_OperationShift | BAM_CIGAR_I; 
                    };                
                    if (trOut.canonSJ[ii-1]>=0 || trOut.sjAnnot[ii-1]==1) {//junction: N

                        packedCIGAR[nCIGAR++]=gapG<<BAM_CIGAR_OperationShift | BAM_CIGAR_N; 
                        SJmotif.push_back(trOut.canonSJ[ii-1] + (trOut.sjAnnot[ii-1]==0 ? 0 : SJ_SAM_AnnotatedMotifShift)); //record junction type
                        SJintron.push_back((int32) (trOut.exons[ii-1][EX_G] + trOut.exons[ii-1][EX_L] + 1 - P->chrStart[trOut.Chr]) );//record intron start
                        SJintron.push_back((int32) (trOut.exons[ii][EX_G] - P->chrStart[trOut.Chr])); //record intron end
                    } else if (gapG>0) {//deletion: N
                        packedCIGAR[nCIGAR++]=gapG<<BAM_CIGAR_OperationShift | BAM_CIGAR_D;                     
                    };
                };                
                packedCIGAR[nCIGAR++]=trOut.exons[ii][EX_L]<<BAM_CIGAR_OperationShift | BAM_CIGAR_M;             
            };

            if (SJmotif.size()==0) {//no junctions recorded, mark with -1
                SJmotif.push_back(-1); 
                SJintron.push_back(-1); 
            };

            uint trimR1=(trOut.exons[iEx1][EX_R]<readLength[leftMate] ? \
                readLengthOriginal[leftMate] : readLength[leftMate]+1+readLengthOriginal[Mate]) \
                - trOut.exons[iEx2][EX_R]-trOut.exons[iEx2][EX_L] - trimL;
            if ( trimR1 > 0 ) {
                packedCIGAR[nCIGAR++]=trimR1<<BAM_CIGAR_OperationShift | BAM_CIGAR_S;             
            };

            MAPQ=P->outSAMmapqUnique;
            if (nTrOut>=5) {
                MAPQ=0;
            } else if (nTrOut>=3) {
                MAPQ=1;
            } else if (nTrOut==2) {
                MAPQ=3;
            };

            //attribute string
            uint tagNM=0;
            string tagMD("");
            if (P->outSAMattrPresent.NM || P->outSAMattrPresent.MD) {
                char* R=Read1[trOut.roStr==0 ? 0:2];
                uint matchN=0;
                for (uint iex=iEx1;iex<=iEx2;iex++) {
                    for (uint ii=0;ii<trOut.exons[iex][EX_L];ii++) {
                        char r1=R[ii+trOut.exons[iex][EX_R]];
                        char g1=G[ii+trOut.exons[iex][EX_G]];
                        if ( r1!=g1 || r1==4 || g1==4) {
                            ++tagNM;
    //                         if (matchN>0 || (ii==0 && iex>0 && trOut.canonSJ[iex]==-1) ) {
                            tagMD+=to_string(matchN);
    //                         };
                            tagMD+=P->genomeNumToNT[(uint8) g1];
                            matchN=0;
                        } else {
                            matchN++;
                        };
                    };
                    if (iex<iEx2) {
                        if (trOut.canonSJ[iex]==-1) {//deletion
                            tagNM+=trOut.exons[iex+1][EX_G]-(trOut.exons[iex][EX_G]+trOut.exons[iex][EX_L]);
                            tagMD+=to_string(matchN) + "^";
                            for (uint ii=trOut.exons[iex][EX_G]+trOut.exons[iex][EX_L];ii<trOut.exons[iex+1][EX_G];ii++) {
                                tagMD+=P->genomeNumToNT[(uint8) G[ii]];
                            };
                            matchN=0;
                        } else if (trOut.canonSJ[iex]==-2) {//insertion
                            tagNM+=trOut.exons[iex+1][EX_R]-trOut.exons[iex][EX_R]-trOut.exons[iex][EX_L];
                        };
                    };
                };      
                tagMD+=to_string(matchN);
            };

            attrN=0;
            for (int ii=0;ii<P->outSAMattrN;ii++) {
                switch (P->outSAMattrOrder[ii]) {
                    case ATTR_NH:
                        attrN+=bamAttrArrayWriteInt(nTrOut,"NH",attrOutArray+attrN,P);
                        break;
                    case ATTR_HI:
                        attrN+=bamAttrArrayWriteInt(iTrOut+1,"HI",attrOutArray+attrN,P);
                        break;
                    case ATTR_AS:
                        attrN+=bamAttrArrayWriteInt(trOut.maxScore,"AS",attrOutArray+attrN,P);
                        break;                    
                    case ATTR_nM:
                        attrN+=bamAttrArrayWriteInt(trOut.nMM,"nM",attrOutArray+attrN,P);
                        break;                            
                    case ATTR_jM:
                        attrN+=bamAttrArrayWrite(SJmotif,"jM",attrOutArray+attrN);                    
                        break;                            
                    case ATTR_jI:
                        attrN+=bamAttrArrayWrite(SJintron,"jI",attrOutArray+attrN);                                        
                        break;
                    case ATTR_XS:
                        if (trOut.sjMotifStrand==1) {
                            attrN+=bamAttrArrayWrite('+',"XS",attrOutArray+attrN);                    
                        } else if (trOut.sjMotifStrand==2) {
                            attrN+=bamAttrArrayWrite('-',"XS",attrOutArray+attrN);                    
                        };                    
                        break;
                    case ATTR_NM:
                        attrN+=bamAttrArrayWriteInt(tagNM,"NM",attrOutArray+attrN,P);
                        break;
                    case ATTR_MD:
                        attrN+=bamAttrArrayWrite(tagMD,"MD",attrOutArray+attrN);                    
                        break;
                    case ATTR_RG:
                        attrN+=bamAttrArrayWrite(P->outSAMattrRG,"RG",attrOutArray+attrN);                    
                        break;                    
                    default:
                        ostringstream errOut;
                        errOut <<"EXITING because of FATAL BUG: unknown/unimplemented SAM atrribute (tag): "<<P->outSAMattrOrder[ii] <<"\n";
                        errOut <<"SOLUTION: contact Alex Dobin at dobin@cshl.edu\n";
                        exitWithError(errOut.str(), std::cerr, P->inOut->logMain, EXIT_CODE_PARAMETER, *P);                         
                };
            };
        };
////////////////////////////// prepare sequence and qualities        
        char seqMate[DEF_readSeqLengthMax+1], qualMate[DEF_readSeqLengthMax+1];
        char *seqOut=NULL, *qualOut=NULL;

        if ( Mate==Str)  {//seq strand is correct, or mate is unmapped
            seqOut=Read0[Mate];
            qualOut=Qual0[Mate];
        } else {
            revComplementNucleotides(Read0[Mate], seqMate, readLengthOriginal[Mate]);
            seqMate[readLengthOriginal[Mate]]=0;
            for (uint ii=0;ii<readLengthOriginal[Mate]; ii++) qualMate[ii]=Qual0[Mate][readLengthOriginal[Mate]-1-ii];
            qualMate[readLengthOriginal[Mate]]=0;
            seqOut=&seqMate[0];
            qualOut=&qualMate[0];            
        };        
        
        //pack sequence
        nuclPackBAM(seqOut,seqMate,readLengthOriginal[Mate]);        
        
/////////////////////////////////// write BAM
        uint32 *pBAM=(uint32*) (outBAM+recSize);
        
        //1: refID: Reference sequence ID, -1 <= refID <= n ref; -1 for a read without a mapping position.       
        if (unmapType<0) {
            pBAM[1]=trOut.Chr;
        } else {
            pBAM[1]=(uint32) -1;
        };
        
        //2: pos: 0-based leftmost coordinate (= POS - 1): int32_t
        if (unmapType<0) {
            pBAM[2]=trOut.exons[iEx1][EX_G] - P->chrStart[trOut.Chr];
        } else {
            pBAM[2]=(uint32) -1;
        };            

        //3: bin mq nl bin<<16|MAPQ<<8|l read name; bin is computed by the > reg2bin() function in Section 4.3; l read name is the length> of read name below (= length(QNAME) + 1).> uint32 t
        if (unmapType<0) {
            pBAM[3]=( ( reg2bin(trOut.exons[iEx1][EX_G] - P->chrStart[trOut.Chr],trOut.exons[iEx2][EX_G] + trOut.exons[iEx2][EX_L] - P->chrStart[trOut.Chr]) << 16 ) \
                   |( MAPQ<<8 ) | ( strlen(readName) ) ); //note:read length includes 0-char
        } else {
            pBAM[3]=( reg2bin(-1,0) << 16 |  strlen(readName) );//4680=reg2bin(-1,0)
        };   
        
        //4: FLAG<<16|n cigar op; n cigar op is the number of operations in CIGAR.
        pBAM[4]=( ( samFLAG << 16 ) | (nCIGAR) ); 

        //5: l seq Length of SEQ
        pBAM[5]=readLengthOriginal[Mate];

        //6: next refID Ref-ID of the next segment (􀀀1  mate refID < n ref)
        if (nMates>1) {
            pBAM[6]=trOut.Chr;
        } else if (mateChr<P->nChrReal){
            pBAM[6]=mateChr;
        } else {
            pBAM[6]=-1;
        };

        //7: next pos 0-based leftmost pos of the next segment (= PNEXT 􀀀 1)
        if (nMates>1) {
            pBAM[7]=trOut.exons[(imate==0 ? iExMate+1 : 0)][EX_G] - P->chrStart[trOut.Chr];
        } else if (mateChr<P->nChrReal){
            pBAM[7]=mateStart-P->chrStart[mateChr];
        } else {
            pBAM[7]=-1;
        };

        //8: tlen Template length (= TLEN)
        if (nMates>1) {
            int32 tlen=trOut.exons[trOut.nExons-1][EX_G]+trOut.exons[trOut.nExons-1][EX_L]-trOut.exons[0][EX_G];
            if (imate>0) tlen=-tlen;
            pBAM[8]=(uint32)tlen;
        } else {
            pBAM[8]=0;
        };            

        recSize+=9*sizeof(int32); //core record size
        
        //Read name1, NULL terminated (QNAME plus a tailing `\0')
        strcpy(outBAM+recSize,readName+1);        
        recSize+=strlen(readName);
        
        //CIGAR: op len<<4|op. `MIDNSHP=X'!`012345678'
        memcpy(outBAM+recSize,packedCIGAR, nCIGAR*sizeof(int32));
        recSize+=nCIGAR*sizeof(int32);
        
        //4-bit encoded read: `=ACMGRSVTWYHKDBN'! [0; 15]; other characters mapped to `N'; high nybble rst (1st base in the highest 4-bit of the 1st byte)
        memcpy(outBAM+recSize,seqMate,(readLengthOriginal[Mate]+1)/2);
        recSize+=(readLengthOriginal[Mate]+1)/2;
        
        //Phred base quality (a sequence of 0xFF if absent)
        if (readFileType==2 && P->outSAMmode != "NoQS") {//output qaultiy
            for (uint32 ii=0; ii<readLengthOriginal[Mate]; ii++) {
                outBAM[recSize+ii]=qualOut[ii]-33;
            };
//             memcpy(outBAM+recSize,qualOut,readLengthOriginal[Mate]);
        } else {
            memset(outBAM+recSize,0xFF,readLengthOriginal[Mate]);
        };
        recSize+=readLengthOriginal[Mate];
        
        //atributes
        memcpy(outBAM+recSize,attrOutArray,attrN);
        recSize+=attrN;
        
        //total size of the record
        pBAM[0]=recSize-recSizePrev-sizeof(int32);//record size exluding the size entry itself
        
        recSizePrev=recSize;
    };//for (uint imate=0;imate<nMates;imate++)
    
    return (uint) recSize;
};

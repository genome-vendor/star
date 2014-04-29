#include "BAMfunctions.h"

void outBAMwriteHeader (BGZF* fp, const string &samh, const vector <string> &chrn, const vector <uint> &chrl) {
    bgzf_write(fp,"BAM\001",4);
    int32 hlen=samh.size();            
    bgzf_write(fp,(char*) &hlen,sizeof(hlen));
    bgzf_write(fp,samh.c_str(),hlen);
    int32 nchr=(int32) chrn.size();
    bgzf_write(fp,(char*) &nchr,sizeof(nchr));
    for (int32 ii=0;ii<nchr;ii++) {
        int32 rlen = (int32) (chrn.at(ii).size()+1);
        int32 slen = (int32) chrl[ii];
        bgzf_write(fp,(char*) &rlen,sizeof(rlen));
        bgzf_write(fp,chrn.at(ii).data(),rlen); //this includes \0 at the end of the string
        bgzf_write(fp,(char*) &slen,sizeof(slen));
    };
    bgzf_flush(fp);
};
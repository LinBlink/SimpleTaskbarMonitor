#include "../SimpleTaskbarMonitor/IfTableSnapshot.h"
#include <cassert>
#include <cstring>
#include <iostream>
#pragma comment(lib, "iphlpapi.lib")
static int mode=0, calls=0;
DWORD WINAPI Query(PMIB_IFTABLE table,PULONG size,BOOL)
{
    ++calls;
    if(mode==3) return ERROR_ACCESS_DENIED;
    if(mode==4) return ERROR_NO_DATA;
    if(mode==5) { *size+=4096; return ERROR_INSUFFICIENT_BUFFER; }
    DWORD rows=(mode==1 || mode==7 || mode==8)?4:mode==2?0:1;
    ULONG required=FIELD_OFFSET(MIB_IFTABLE,table)+rows*sizeof(MIB_IFROW);
    if(*size<required) {*size=required;return ERROR_INSUFFICIENT_BUFFER;}
    memset(table,0,*size);table->dwNumEntries=rows;
    for(DWORD i=0;i<rows;++i) {table->table[i].dwIndex=i+1;table->table[i].dwOperStatus=IF_OPER_STATUS_OPERATIONAL;}
    if(mode==7) table->table[0].dwOperStatus=IF_OPER_STATUS_DISCONNECTED;
    if(mode==8) for(DWORD i=0;i<rows;++i) table->table[i].dwIndex=rows-i;
    if(mode==6) table->dwNumEntries=1000000;
    *size=required;return NO_ERROR;
}
int main()
{
    CIfTableSnapshot snapshot;
    assert(snapshot.Refresh(Query));assert(snapshot.Get()->dwNumEntries==1);assert(snapshot.Changed());
    assert(snapshot.Refresh(Query));assert(!snapshot.Changed());
    mode=1;calls=0;assert(snapshot.Refresh(Query));assert(calls>1);assert(snapshot.Get()->dwNumEntries==4);assert(snapshot.Changed());
    mode=7;assert(snapshot.Refresh(Query));assert(snapshot.Changed());
    assert(snapshot.Refresh(Query));assert(!snapshot.Changed());
    mode=8;assert(snapshot.Refresh(Query));assert(snapshot.Changed());
    mode=2;assert(snapshot.Refresh(Query));assert(snapshot.Get()->dwNumEntries==0);assert(snapshot.Changed());
    mode=0;assert(snapshot.Refresh(Query));
    mode=3;assert(!snapshot.Refresh(Query));assert(snapshot.Get()->dwNumEntries==0);assert(snapshot.Error()==ERROR_ACCESS_DENIED);
    mode=4;assert(snapshot.Refresh(Query));assert(snapshot.Get()->dwNumEntries==0);
    mode=5;calls=0;assert(!snapshot.Refresh(Query));assert(calls==8);assert(snapshot.Get()->dwNumEntries==0);
    mode=6;assert(!snapshot.Refresh(Query));assert(snapshot.Error()==ERROR_INVALID_DATA);assert(snapshot.Get()->dwNumEntries==0);
    mode=0;assert(snapshot.Refresh(Query));assert(snapshot.Get()->dwNumEntries==1);
    std::cout << "PASS: growth, shrink, link-state changes, row reorder, empty, API failure, churn, invalid rows, recovery\n";
}

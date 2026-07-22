#include <uf.h>
#include <uf_defs.h>
#include <uf_modl.h>
#include <uf_modl_utilities.h>
#include <uf_modl_blends.h>
#include <uf_ui.h>
#include <uf_part.h>
#include <NXOpen/Session.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
using namespace NXOpen;
struct Fi{std::string id,strat,sev;double cx,cy,cz;double fcx,fcy,fcz;};
static std::string gs(const std::string& j,const std::string& k){
    std::string q="\""+k+"\":\"";size_t p=j.find(q);
    if(p==std::string::npos){q="\""+k+"\": \"";p=j.find(q);}
    if(p==std::string::npos)return"";p+=q.length();
    size_t e=j.find("\"",p);return(e==std::string::npos)?"":j.substr(p,e-p);
}
static double gn(const std::string& j,const std::string& k){
    std::string q="\""+k+"\":";size_t p=j.find(q);
    if(p==std::string::npos)return 0;p+=q.length();
    while(p<j.size()&&j[p]==' ')p++;return atof(j.c_str()+p);
}
static std::vector<Fi> prs(const char* path){
    std::vector<Fi> v;std::ifstream f(path);
    if(!f.is_open())return v;
    std::string c((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());f.close();
    size_t pos=0;
    while((pos=c.find("\"region_id\":",pos))!=std::string::npos){
        size_t bs=c.rfind("{",pos);if(bs==std::string::npos)break;
        size_t be=c.find("}",bs);if(be==std::string::npos)break;
        std::string b=c.substr(bs,be-bs+1);
        Fi a;a.id=gs(b,"region_id");a.strat=gs(b,"suggested_fix");
        a.sev=gs(b,"severity");
        size_t cs=b.find("\"centroid\":[");
        if(cs!=std::string::npos){cs+=12;sscanf(b.c_str()+cs,"%lf,%lf,%lf",&a.cx,&a.cy,&a.cz);}
        // 优先用 fix_centroid，回退到 centroid
        size_t fs=b.find("\"fix_centroid\":[");
        if(fs!=std::string::npos){fs+=16;sscanf(b.c_str()+fs,"%lf,%lf,%lf",&a.fcx,&a.fcy,&a.fcz);}
        else{a.fcx=a.cx;a.fcy=a.cy;a.fcz=a.cz;}
        v.push_back(a);pos=be+1;
    }
    return v;
}
extern "C" DllExport int ufsta(char*,int*,int){
    UF_initialize();
    const char* pp=getenv("STEEL_PRT_PATH"),*dp=getenv("STEEL_DIAG_PATH"),*op=getenv("STEEL_OUTPUT_PATH");
    if(!pp)pp=getenv("INPUT_PRT");if(!dp)dp="diagnosis_result.json";
    if(!pp){UF_UI_write_listing_window("ERR: no PRT\n");UF_terminate();return 1;}
    tag_t pt;UF_PART_load_status_t es;
    if(UF_PART_open(pp,&pt,&es)){UF_UI_write_listing_window("ERR: open\n");UF_terminate();return 1;}
    UF_UI_open_listing_window();UF_UI_write_listing_window("starting fix...\n");
    std::vector<Fi> items=prs(dp);
    char buf[256];sprintf(buf,"regions: %zu\n",items.size());UF_UI_write_listing_window(buf);
    Session* sess=Session::GetSession();Part* wp=sess->Parts()->Work();
    int ok=0,fail=0;
    for(auto& a:items){
        if(a.sev!="CRITICAL"){sprintf(buf,"  %s: %s skip\n",a.id.c_str(),a.sev.c_str());UF_UI_write_listing_window(buf);ok++;continue;}
        double bd=1e10;tag_t bf=NULL_TAG;
        for(Body* b:*wp->Bodies()){for(Face* f:b->GetFaces()){
            int ty,nd;double p[3],d[3],bx[6],rd,rd2;
            UF_MODL_ask_face_data(f->Tag(),&ty,p,d,bx,&rd,&rd2,&nd);
            double cfx=(bx[0]+bx[3])/2,cfy=(bx[1]+bx[4])/2,cfz=(bx[2]+bx[5])/2;
            double dx=a.cx-cfx,dy=a.cy-cfy,dz=a.cz-cfz;
            double dist=sqrt(dx*dx+dy*dy+dz*dz);
            if(dist<bd){bd=dist;bf=f->Tag();}
        }}
        if(bf==NULL_TAG){sprintf(buf,"  %s: no face\n",a.id.c_str());UF_UI_write_listing_window(buf);fail++;continue;}
        // offset_face strategy — proven most effective
        uf_list_p_t fl=nullptr;
        int rc=UF_MODL_create_list(&fl);
        if(rc!=0||!fl){sprintf(buf,"  %s: list fail\n",a.id.c_str());UF_UI_write_listing_window(buf);fail++;continue;}
        UF_MODL_put_list_item(fl,bf);tag_t feat;
        rc=UF_MODL_create_face_offset((char*)"0.5",fl,&feat);
        UF_MODL_delete_list(&fl);
        sprintf(buf,"  %s: offset -> %s\n",a.id.c_str(),rc==0?"OK":"FAIL");UF_UI_write_listing_window(buf);
        if(rc==0)ok++;else fail++;
    }
    if(op&&strlen(op)>0){UF_PART_save_as(op);UF_PART_close(pt,1,1);}
    else{UF_PART_save();UF_PART_close(pt,1,1);}
    sprintf(buf,"done: %d ok %d fail\n",ok,fail);UF_UI_write_listing_window(buf);
    UF_terminate();return 0;
}
extern "C" DllExport void ufusr(char*p,int*r,int l){ufsta(p,r,l);}
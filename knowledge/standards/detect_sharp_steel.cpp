#include <uf.h>
#include <uf_defs.h>
#include <uf_modl.h>
#include <uf_modl_utilities.h>
#include <uf_ui.h>
#include <uf_part.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <NXOpen/Session.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
using namespace NXOpen;

struct Fi {
    double cx, cy, cz;    // face centroid
    double bx[6];          // bounding box
    double ar, th;         // aspect ratio, thinness
    int fc;                // face count (1 for single face)
    std::string sev, fix;  // severity, suggested fix
    double fcx, fcy, fcz;  // primary face centroid for fix targeting
};
static std::string ts() {
    time_t t = time(nullptr); char b[64];
    strftime(b,64,"%Y-%m-%dT%H:%M:%S",localtime(&t)); return std::string(b);
}
static std::string esc(const std::string& s) {
    std::string o;
    for(char c:s){if(c=='"')o+="\\\"";else if(c=='\n')o+="\\n";else if(c=='\\')o+="\\\\";else o+=c;}
    return o;
}
static std::string sev(double ar, double th) {
    if(ar>200||th>500)return"CRITICAL"; if(ar>80||th>200)return"WARNING";
    if(ar>20||th>80)return"INFO"; return"PASS";
}

extern "C" DllExport int ufsta(char*,int*,int) {
    UF_initialize();
    const char* pp=getenv("STEEL_PRT_PATH"),*op=getenv("STEEL_OUTPUT_PATH");
    if(!pp)pp=getenv("INPUT_PRT"); if(!op)op="diagnosis_result.json";
    if(!pp){UF_UI_write_listing_window("ERR: no PRT\n");UF_terminate();return 1;}

    tag_t pt; UF_PART_load_status_t es;
    if(UF_PART_open(pp,&pt,&es)){UF_UI_write_listing_window("ERR: open\n");UF_terminate();return 1;}

    std::vector<Fi> fa; // all faces
    for(Body* b:*Session::GetSession()->Parts()->Work()->Bodies()){
        for(Face* f:b->GetFaces()){
            tag_t ft=f->Tag(); int ty,nd; double p[3],d[3],bx[6],rd,rd2;
            UF_MODL_ask_face_data(ft,&ty,p,d,bx,&rd,&rd2,&nd);
            uf_list_p_t el=nullptr; UF_MODL_ask_face_edges(ft,&el);
            int ne=0; if(el)UF_MODL_ask_list_count(el,&ne);
            double pm=0;
            for(int i=0;i<ne;i++){
                tag_t et; UF_MODL_ask_list_item(el,i,&et);
                double a[3],bb[3]; int nv;
                if(!UF_MODL_ask_edge_verts(et,a,bb,&nv)&&nv>0){
                    double dx=bb[0]-a[0],dy=bb[1]-a[1],dz=bb[2]-a[2];
                    pm+=sqrt(dx*dx+dy*dy+dz*dz);
                }
            }
            if(el)UF_MODL_delete_list(&el);
            double dx=bx[3]-bx[0],dy=bx[4]-bx[1],dz=bx[5]-bx[2];
            double dm[3]={dx,dy,dz}; std::sort(dm,dm+3);
            double md=dm[0]>0.001?dm[0]:0.001, ar=dm[2]/md;
            double ae=2*(dx*dy+dy*dz+dz*dx), th=(pm>0&&ae>0.001)?(pm*pm)/ae:0;
            std::string s=sev(ar,th);
            Fi f2; f2.cx=(bx[0]+bx[3])/2; f2.cy=(bx[1]+bx[4])/2; f2.cz=(bx[2]+bx[5])/2;
            memcpy(f2.bx,bx,24); f2.ar=ar; f2.th=th; f2.fc=1; f2.sev=s;
            f2.fcx=f2.cx; f2.fcy=f2.cy; f2.fcz=f2.cz; // exact centroid
            f2.fix=(s=="CRITICAL"?"add_material_patch":s=="WARNING"?"offset_face":"review_only");
            fa.push_back(f2);
        }
    }

    int cP=0,cI=0,cW=0,cC=0;
    for(auto& f:fa){if(f.sev=="PASS")cP++;else if(f.sev=="INFO")cI++;else if(f.sev=="WARNING")cW++;else if(f.sev=="CRITICAL")cC++;}

    std::vector<int> fi;
    for(size_t i=0;i<fa.size();i++)if(fa[i].sev!="PASS")fi.push_back((int)i);
    std::vector<bool> u(fi.size(),false);
    std::vector<Fi> rs;

    for(size_t i=0;i<fi.size();i++){
        if(u[i])continue;
        Fi r=fa[fi[i]]; u[i]=true;
        // r.fcx/y/z is already the first face's exact centroid
        bool ch;
        do{
            ch=false;
            for(size_t j=0;j<fi.size();j++){
                if(u[j])continue;
                double* b1=r.bx,*b2=fa[fi[j]].bx;
                double ovX=std::min(b1[3],b2[3])-std::max(b1[0],b2[0]);
                double ovY=std::min(b1[4],b2[4])-std::max(b1[1],b2[1]);
                double wX1=b1[3]-b1[0],wX2=b2[3]-b2[0],wY1=b1[4]-b1[1],wY2=b2[4]-b2[1];
                if(ovX>std::min(wX1,wX2)*0.3&&ovY>std::min(wY1,wY2)*0.3){
                    r.fc++;
                    for(int k=0;k<3;k++){if(b2[k]<r.bx[k])r.bx[k]=b2[k];if(b2[k+3]>r.bx[k+3])r.bx[k+3]=b2[k+3];}
                    if(fa[fi[j]].th>r.th)r.th=fa[fi[j]].th;
                    if(fa[fi[j]].ar>r.ar)r.ar=fa[fi[j]].ar;
                    u[j]=true;ch=true;
                }
            }
        }while(ch);
        r.cx=(r.bx[0]+r.bx[3])/2; r.cy=(r.bx[1]+r.bx[4])/2; r.cz=(r.bx[2]+r.bx[5])/2;
        r.sev=sev(r.ar,r.th);
        r.fix=(r.sev=="CRITICAL"?"add_material_patch":r.sev=="WARNING"?"offset_face":"review_only");
        rs.push_back(r);
    }

    std::stringstream j;
    j<<"{\n \"prt_path\":\""<<esc(pp)<<"\",\n \"timestamp\":\""<<ts()<<"\",\n \"analysis_summary\":{";
    j<<"\"total_faces\":"<<(int)fa.size()<<",\"flagged_faces\":"<<cW+cC<<",\"sharp_steel_regions\":"<<(int)rs.size()<<",\"overall_verdict\":\""<<(cC>0?"FAIL":cW>0?"WARNING":"PASS")<<"\"},\n";
    j<<" \"sharp_steel_regions\":[\n";
    for(size_t i=0;i<rs.size();i++){
        if(i)j<<",\n";
        char id[32]; sprintf(id,"%03d",(int)(i+1));
        j<<"   {\"region_id\":\"SS-"<<id<<"\",\"severity\":\""<<rs[i].sev<<"\",\"aspect_ratio\":"<<rs[i].ar<<",\"thinness\":"<<rs[i].th<<",\"face_count\":"<<rs[i].fc<<",\"centroid\":["<<rs[i].cx<<","<<rs[i].cy<<","<<rs[i].cz<<"]";
        // ★ fix_centroid: exact centroid of the primary face in this region
        j<<",\"fix_centroid\":["<<rs[i].fcx<<","<<rs[i].fcy<<","<<rs[i].fcz<<"]";
        j<<",\"suggested_fix\":\""<<rs[i].fix<<"\"}";
    }
    j<<"\n ],\n \"face_statistics\":{\"pass\":"<<cP<<",\"info\":"<<cI<<",\"warning\":"<<cW<<",\"critical\":"<<cC<<"},\n";
    j<<" \"recommendation\":\""<<(rs.empty()?"PASS":"检测到"+std::to_string(rs.size())+"个尖钢区域")<<"\"\n}\n";

    std::string js=j.str();
    std::ofstream f(op); if(f.is_open()){f<<js;f.close();}
    UF_UI_open_listing_window(); UF_UI_write_listing_window(js.c_str());
    UF_PART_close(pt,1,1); UF_terminate();
    return 0;
}
extern "C" DllExport void ufusr(char*p,int*r,int l){ufsta(p,r,l);}
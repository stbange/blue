#include "control.h"
#include "cJSON.h"
#include "lua.h"
#include "lauxlib.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { size_t used; BluePatch *patch; } Script;
static void *limited(void *ud,void *ptr,size_t old,size_t n){
    Script *s=ud;if(!ptr)old=0;
    if(!n){free(ptr);s->used-=old;return NULL;}
    if(n>524288 || s->used-old>524288-n)return NULL;
    void *p=realloc(ptr,n);if(p)s->used=s->used-old+n;return p;
}
static void budget(lua_State *L,lua_Debug *ar){(void)ar;luaL_error(L,"instruction budget exceeded");}
static int route(lua_State *L){
    static const char *const dest[]={"gain","morph","cutoff","density",NULL};
    Script *s;lua_getallocf(L,(void **)&s);
    lua_Integer index=luaL_checkinteger(L,1);int k=luaL_checkoption(L,2,NULL,dest);
    double val=luaL_checknumber(L,3);
    if(index<1 || index>8 || !isfinite(val) || fabs(val)>16)return luaL_error(L,"invalid macro route");
    s->patch->routes[index-1][k]=(float)val;return 0;
}
static const char *const modes[]={"sine","saw","table","fm","ring","sample","grain",NULL};
static const char *const inserts[]={"bypass","lowpass","highpass","bandpass","shape","comb",NULL};
static int choice(const char *const *list,const char *name){if(name)for(int i=0;list[i];i++)if(!strcmp(name,list[i]))return i;return -1;}
static int layer(lua_State *L){
    Script *s;lua_getallocf(L,(void **)&s);lua_Integer i=luaL_checkinteger(L,1);
    int mode=luaL_checkoption(L,2,NULL,modes);double gain=luaL_checknumber(L,3);
    if(i<1 || i>3 || !isfinite(gain) || gain<0 || gain>2)return luaL_error(L,"invalid lane");
    s->patch->lane[i-1].source=mode;s->patch->lane[i-1].gain=(float)gain;return 0;
}
static int safe(const char *s){
    if(!s || !*s || *s=='/' || strstr(s,"..") || strstr(s,"//"))return 0;
    for(;*s;s++)if(!isalnum((unsigned char)*s) && !strchr("-_./",*s))return 0;
    return 1;
}
static char *read_text(const char *path){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
    if(n<0 || n>1048576){fclose(f);return NULL;}rewind(f);
    char *s=malloc((size_t)n+1);if(s && fread(s,1,n,f)!=(size_t)n){free(s);s=NULL;}
    if(s)s[n]=0;fclose(f);return s;
}
static cJSON *json(const char *path){char *s=read_text(path);cJSON *j=s?cJSON_ParseWithOpts(s,NULL,1):NULL;free(s);return j;}
#define FIELD(o,k) cJSON_GetObjectItemCaseSensitive(o,k)
static void number(cJSON *o,const char *k,float *dst,double min,double max,int *ok){
    cJSON *v=FIELD(o,k);if(!v)return;
    if(!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<min || v->valuedouble>max){*ok=0;return;}*dst=(float)v->valuedouble;
}
static int path_for(char *path,size_t n,const char *root,const char *rel){
    if(!safe(rel))return 0;int m=snprintf(path,n,"%s/%s",root,rel);return m>=0 && (size_t)m<n;
}
int blue_load(BluePatch *out,const char *folder,const char *preset,char *err,size_t size){
    char path[2048]="pack.json";int ok=0,valid=1;
    cJSON *pack=NULL,*doc=NULL;BluePatch *p=malloc(sizeof *p);
    const char *reason="invalid or missing document";char script_error[180]="";
    if(!p || !folder || !preset || !path_for(path,sizeof path,folder,"pack.json"))goto end;
    blue_defaults(p);pack=json(path);
    if(!pack || cJSON_GetNumberValue(FIELD(pack,"format"))!=1 || cJSON_GetNumberValue(FIELD(pack,"luaApi"))!=1)goto end;
    const char *rel=cJSON_GetStringValue(FIELD(FIELD(pack,"presets"),preset));
    if(!path_for(path,sizeof path,folder,rel))goto end;
    doc=json(path);if(!doc || cJSON_GetNumberValue(FIELD(doc,"format"))!=1)goto end;
    cJSON *lanes=FIELD(doc,"lanes");int count=cJSON_GetArraySize(lanes);
    if(count<1 || count>3)goto end;
    #define NUM(obj,field,lo,hi) number(obj,#field,&p->field,lo,hi,&valid)
    NUM(doc,gain,0,1);NUM(doc,attack,0,30);NUM(doc,release,0,30);NUM(doc,bpm,20,300);NUM(doc,delay,.01,4);NUM(doc,feedback,0,.98);NUM(doc,reverb,0,.98);NUM(doc,echo_pitch,-24,24);
    for(int i=0;i<count;i++){
        cJSON *o=cJSON_GetArrayItem(lanes,i);BlueLane *l=&p->lane[i];
        l->source=choice(modes,cJSON_GetStringValue(FIELD(o,"source")));if(l->source<0)goto end;
        #define LNUM(k,lo,hi) number(o,#k,&l->k,lo,hi,&valid)
        LNUM(gain,0,2);LNUM(pan,-1,1);LNUM(ratio,.01,32);LNUM(depth,0,32);LNUM(cutoff,10,24000);
        LNUM(send,0,1);LNUM(space,0,1);LNUM(morph,0,1);LNUM(position,0,1);LNUM(scatter,0,1);LNUM(size,.001,1);LNUM(density,1,256);
        cJSON *ins=FIELD(o,"inserts");if(cJSON_GetArraySize(ins)>2)goto end;
        for(int k=0;k<cJSON_GetArraySize(ins);k++){l->insert[k]=choice(inserts,cJSON_GetStringValue(cJSON_GetArrayItem(ins,k)));if(l->insert[k]<0)goto end;}
        cJSON *waves=FIELD(o,"harmonics");
        for(int k=0;k<cJSON_GetArraySize(waves);k++){
            cJSON *row=cJSON_GetArrayItem(waves,k);if(k>=2 || cJSON_GetArraySize(row)>16)goto end;
            memset(l->harmonic[k],0,sizeof l->harmonic[k]);
            for(int h=0;h<cJSON_GetArraySize(row);h++){
                double x=cJSON_GetNumberValue(cJSON_GetArrayItem(row,h));if(!isfinite(x)||fabs(x)>2)goto end;l->harmonic[k][h]=(float)x;
            }
        }
    }
    cJSON *env=FIELD(doc,"envelope");p->points=cJSON_GetArraySize(env);if(p->points>BLUE_POINTS)goto end;
    for(int i=0;i<p->points;i++){
        cJSON *o=cJSON_GetArrayItem(env,i);BluePoint *pt=&p->envelope[i];pt->curve=1;
        number(o,"time",&pt->time,0,3600,&valid);number(o,"value",&pt->value,0,1,&valid);number(o,"curve",&pt->curve,.05,20,&valid);
        if((i==0 && pt->time!=0)||(i && pt->time<=p->envelope[i-1].time))goto end;
    }
    if(FIELD(doc,"loop")){double x=cJSON_GetNumberValue(FIELD(doc,"loop"));if(!isfinite(x)||x<0||x>=p->points||floor(x)!=x)goto end;p->loop=(int)x;}
    p->sync=cJSON_IsTrue(FIELD(doc,"sync"));
    if(!valid)goto end;
    if(FIELD(doc,"sample")){
        rel=cJSON_GetStringValue(FIELD(FIELD(pack,"assets"),cJSON_GetStringValue(FIELD(doc,"sample"))));
        if(!path_for(path,sizeof path,folder,rel) || !blue_read_sample(path,p))goto end;
    }
    rel=cJSON_GetStringValue(FIELD(pack,"script"));if(!path_for(path,sizeof path,folder,rel))goto end;
    char *src=read_text(path);if(!src)goto end;
    Script state={0,p};lua_State *L=lua_newstate(limited,&state);if(!L){free(src);goto end;}
    lua_newtable(L);lua_pushcfunction(L,route);lua_setfield(L,-2,"route");lua_pushcfunction(L,layer);lua_setfield(L,-2,"lane");lua_setglobal(L,"blue");
    lua_sethook(L,budget,LUA_MASKCOUNT,50000);
    int rc=luaL_loadbufferx(L,src,strlen(src),rel,"t");free(src);if(!rc)rc=lua_pcall(L,0,0,0);
    if(rc){snprintf(script_error,sizeof script_error,"%s",lua_tostring(L,-1));reason=script_error;}
    lua_close(L);if(rc)goto end;
    *out=*p;ok=1;
end:
    if(!ok && size)snprintf(err,size,"%s (%s): %s",path,preset?preset:"",reason);
    free(p);cJSON_Delete(pack);cJSON_Delete(doc);return ok;
}

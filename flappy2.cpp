// ============================================================
//  Wing It v5.0
//  Copyright (c) 2026 Ayoub Assouar (EvenKiller0). All rights reserved.
//  Unauthorized copying, reverse engineering, decompilation,
//  or distribution of this software is strictly prohibited.
// ============================================================
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <mmsystem.h>
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"advapi32.lib")
#include "sounds.h"
#include "icon_data.h"

static const volatile char _CR[] =
    "\xA9 2026 Ayoub Assouar (EvenKiller0 / EvenKiller01YT). "
    "All rights reserved. Unauthorized reverse engineering prohibited.";

// ====================== ANTI-CHEAT / DRM =========================
static bool g_tampered  = false;
static int  g_speedHits = 0;
#define SCORE_XOR   0xA5C3E7B1u
#define INTEG_MAGIC 0xDEADF1A9u
#define COINS_XOR   0x3F7A12C6u

static DWORD encodeScore(int s){ return (DWORD)s ^ SCORE_XOR; }
static int   decodeScore(DWORD d){ return (int)(d ^ SCORE_XOR); }
static DWORD encodeCoins(int c){ return (DWORD)c ^ COINS_XOR; }
static int   decodeCoins(DWORD d){ return (int)(d ^ COINS_XOR); }

static bool debuggerPresent(){
    if(IsDebuggerPresent()) return true;
    BOOL rem=FALSE; CheckRemoteDebuggerPresent(GetCurrentProcess(),&rem); return rem==TRUE;
}
static LONGLONG g_qpcFreq=0,g_qpcPrev=0; static DWORD g_tickPrev=0;
static void initTiming(){
    LARGE_INTEGER f,c; QueryPerformanceFrequency(&f); g_qpcFreq=f.QuadPart;
    QueryPerformanceCounter(&c); g_qpcPrev=c.QuadPart; g_tickPrev=GetTickCount();
}
static void checkSpeedHack(){
    if(!g_qpcFreq) return;
    LARGE_INTEGER c; QueryPerformanceCounter(&c); DWORD now=GetTickCount();
    double qpcMs=(double)(c.QuadPart-g_qpcPrev)*1000.0/(double)g_qpcFreq;
    double tickMs=(double)(int)(now-g_tickPrev);
    g_qpcPrev=c.QuadPart; g_tickPrev=now;
    if(tickMs>50.0){ double r=qpcMs/(tickMs+0.001);
        if(r<0.5||r>2.0){if(++g_speedHits>=3)g_tampered=true;}
        else if(g_speedHits>0)g_speedHits--; }
}
static DWORD g_integRef=0; static bool g_integSet=false;
static void  setInteg(int b){ g_integRef=(DWORD)b^INTEG_MAGIC^SCORE_XOR; g_integSet=true; }
static bool  chkInteg(int b){ return !g_integSet||(((DWORD)b^INTEG_MAGIC^SCORE_XOR)==g_integRef); }
// =================================================================

// ====================== SOUND ==================================
struct SA{const unsigned char*d;DWORD l;};
static DWORD WINAPI sndThrd(LPVOID a){ SA*x=(SA*)a;
    PlaySoundA((LPCSTR)x->d,NULL,SND_MEMORY|SND_ASYNC|SND_NODEFAULT); delete x; return 0; }
static void playSound(const unsigned char*d,DWORD l){
    SA*a=new SA{d,l}; HANDLE h=CreateThread(NULL,0,sndThrd,a,0,NULL); if(h)CloseHandle(h); }
static void playSoundThread(const unsigned char*d,DWORD l){ playSound(d,l); }

// ====================== CONSTANTS ==============================
#define WW 400
#define WH 600
#define BIRD_X    90
#define BIRD_W    34
#define BIRD_H    24
#define GRAVITY   0.45f
#define JUMP_VEL  -8.0f
#define MAX_FALL  10.0f
#define PIPE_W    60
#define PIPE_GAP  150
#define BASE_PIPE_SPEED 2.8f
#define PIPE_COUNT 3
#define PIPE_SPACE 180
#define GROUND_Y  480
#define GROUND_H  (WH-GROUND_Y)

// ====================== ABILITIES ==============================
// 4 abilities: 0=Ghost 1=SlowMo 2=Heart 3=Dash
// Buy = +1 use. Each use lasts a fixed duration. Must rebuy after using.
#define ABILITY_COUNT 4
static const char* ABILITY_NAMES[ABILITY_COUNT] = {"GHOST","SLOW MO","HEART","DASH"};
static const char* ABILITY_DESC[ABILITY_COUNT]  = {
    "Pass through pipes",
    "Halve pipe speed",
    "Extra life (revive once)",
    "Dash through a gap"
};
static const int ABILITY_COST[ABILITY_COUNT]  = {120, 150, 200, 80}; // cost per 1 use
static const int ABILITY_DUR[ABILITY_COUNT]   = {300, 360, 99999, 1}; // frames (heart=permanent; dash=instant 1 frame)
static const int ABILITY_MAX_STACK = 5;                               // max uses you can hold
// Dash: how many pixels to teleport the bird forward (enough to clear a pipe gap)
#define DASH_DIST 165

struct Ability {
    int uses;       // how many uses bought (0 = none)
    int active;     // frames remaining active (0 = inactive)
    int cooldown;   // frames until next use allowed (0 = ready)
    // no cooldown between uses for most; dash has explicit cooldown
};

// ====================== OPTIONS ================================
static int g_fpsChoice = 1; // 0=30 1=60 2=120
static const int FPS_VALUES[3] = {30, 60, 120};
static const char* FPS_LABELS[3] = {"30", "60", "120"};
static void saveOptions(){
    HKEY hk;
    if(RegCreateKeyExA(HKEY_CURRENT_USER,"Software\\WingIt47",0,NULL,0,KEY_WRITE,NULL,&hk,NULL)==ERROR_SUCCESS){
        DWORD v=(DWORD)g_fpsChoice;
        RegSetValueExA(hk,"FpsChoice",0,REG_DWORD,(BYTE*)&v,sizeof(DWORD));
        RegCloseKey(hk);
    }
}
static void loadOptions(){
    HKEY hk;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,"Software\\WingIt47",0,KEY_READ,&hk)==ERROR_SUCCESS){
        DWORD v=1,sz=sizeof(DWORD),t=REG_DWORD;
        if(RegQueryValueExA(hk,"FpsChoice",NULL,&t,(BYTE*)&v,&sz)==ERROR_SUCCESS)
            g_fpsChoice=(int)v<3?(int)v:1;
        RegCloseKey(hk);
    }
}

// ====================== GAME STATE ============================
enum State{IDLE,PLAYING,DEAD,MENU,PAUSE,SHOP,OPTIONS};
struct Pipe{float x; int topH; bool scored;};

// ====================== BG ANIMATION ===========================
struct BgAnim {
    float pipeX[PIPE_COUNT];
    int   pipeTopH[PIPE_COUNT];
    float birdY, birdVY;
    int   bobFrame;
    void init(){
        for(int i=0;i<PIPE_COUNT;i++){
            pipeX[i]=(float)(WW+i*PIPE_SPACE);
            pipeTopH[i]=80+rand()%(GROUND_Y-PIPE_GAP-100);
        }
        birdY=WH/2.f; birdVY=0; bobFrame=0;
    }
    void update(){
        bobFrame++;
        birdY += sinf(bobFrame*0.05f)*1.2f;
        birdY = std::max(60.f, std::min(birdY, (float)GROUND_Y-30));
        for(int i=0;i<PIPE_COUNT;i++){
            pipeX[i]-=BASE_PIPE_SPEED;
            if(pipeX[i]+PIPE_W<0){
                float mx=0;
                for(int j=0;j<PIPE_COUNT;j++) if(pipeX[j]>mx) mx=pipeX[j];
                pipeX[i]=mx+PIPE_SPACE;
                pipeTopH[i]=80+rand()%(GROUND_Y-PIPE_GAP-100);
            }
        }
    }
} g_bg;

struct Game {
    State state;
    float birdY, birdVY, birdAngle;
    Pipe  pipes[PIPE_COUNT];
    int   score, best;
    int   coins, totalCoins; // coins this run, total saved
    DWORD deathTime;
    float pipeSpeed;
    Ability abilities[ABILITY_COUNT];
    int   shopCursor;
    int   optCursor;    // options menu cursor
    State prevState;    // state before OPTIONS opened
    // Points earned this run (shown on death screen)
    int   earnedCoins;
    // Slow mo tint alpha (0-60)
    int   slowTint;
    // Dash animation: frames remaining, distance left to travel, trail positions
    int   dashFrames;
    float dashDistLeft;
    float dashTrailX[5]; // bird ghost trail X offsets (drawn behind bird)
    float dashTrailY[5];
    int   dashTrailCount;
    // Heart safety system
    float lastSafeY;      // last Y where bird wasn't near pipes or walls
    int   invincFrames;   // frames of post-heart invincibility (bird flashes, pipes pass through)
    int   shopScroll;     // shop scroll offset in pixels

    void initAbilities(){
        for(int i=0;i<ABILITY_COUNT;i++){
            abilities[i].active=0;
            abilities[i].cooldown=0;
            // uses persist between runs — don't reset them
        }
    }

    void init(){
        state=MENU; score=0; coins=0; earnedCoins=0;
        birdY=WH/2.f; birdVY=0; birdAngle=0;
        pipeSpeed=BASE_PIPE_SPEED; slowTint=0; optCursor=0; shopScroll=0;
        initAbilities();
        spawnPipes();
        g_bg.init();
    }

    void spawnPipes(){
        for(int i=0;i<PIPE_COUNT;i++){
            pipes[i].x=(float)(WW+i*PIPE_SPACE);
            pipes[i].topH=80+rand()%(GROUND_Y-PIPE_GAP-100);
            pipes[i].scored=false;
        }
    }

    // coins earned = score * (1 + score/10) capped
    int calcCoins(int sc){
        int c = sc + sc*sc/20;
        return c > 9999 ? 9999 : c;
    }

    void activateAbility(int idx){
        if(state!=PLAYING) return;
        Ability& ab=abilities[idx];
        if(ab.uses<=0) return;          // none bought
        if(ab.cooldown>0) return;       // on cooldown
        if(ab.active>0) return;         // already active
        if(idx==3){
            // DASH — animated: move pipes backward over ~20 frames
            ab.uses--;
            ab.cooldown = 90; // 1.5s cooldown at 60fps
            dashFrames   = 20;        // animation lasts 20 physics ticks
            dashDistLeft = DASH_DIST; // total pipe movement to distribute
            dashTrailCount = 0;
            playSound(SND_SWOOSH, SND_SWOOSH_LEN);
            return;
        }
        ab.uses--;
        ab.active = ABILITY_DUR[idx];
        playSound(SND_SWOOSH, SND_SWOOSH_LEN);
    }

    void startGame(){
        state=PLAYING;
        playSound(SND_SWOOSH,SND_SWOOSH_LEN);
        birdY=WH/2.f; birdVY=JUMP_VEL; birdAngle=0;
        spawnPipes(); score=0; coins=0; slowTint=0;
        initAbilities();
    }
    void jump(){
        if(state==PLAYING){
            birdVY=JUMP_VEL; playSound(SND_FLAP,SND_FLAP_LEN);
        } else if(state==DEAD&&GetTickCount()-deathTime>=1400){
            g_bg.init(); state=MENU; // init fresh bg when leaving death screen
        }
    }

    bool hitRect(RECT a,RECT b){
        return a.left<b.right&&a.right>b.left&&a.top<b.bottom&&a.bottom>b.top;
    }

    void update(){
        if(state==MENU||state==PAUSE) g_bg.update();
        // DEAD: bg frozen on real game pipes — no update
        if(state!=PLAYING) return;
        checkSpeedHack();
        if(!chkInteg(best)){ g_tampered=true; best=0; setInteg(0); }
        // Tick invincibility
        if(invincFrames>0) invincFrames--;

        // Update ability timers
        bool ghost=false, slowmo=false;
        for(int i=0;i<ABILITY_COUNT;i++){
            if(abilities[i].cooldown>0) abilities[i].cooldown--;
            if(abilities[i].active>0 && i!=2) abilities[i].active--; // heart never ticks down
            if(i==0 && abilities[i].active>0) ghost=true;
            if(i==1 && abilities[i].active>0) slowmo=true;
        }
        // Dash animation — move pipes backward over dashFrames ticks
        if(dashFrames>0){
            // Ease-out: move more at start, less at end
            float step = dashDistLeft / (float)dashFrames;
            for(int i=0;i<PIPE_COUNT;i++) pipes[i].x -= step;
            dashDistLeft -= step;
            // Store trail: shift array and record current bird pos
            if(dashTrailCount < 5) dashTrailCount++;
            for(int t=dashTrailCount-1;t>0;t--){
                dashTrailX[t]=dashTrailX[t-1];
                dashTrailY[t]=dashTrailY[t-1];
            }
            dashTrailX[0]=(float)BIRD_X;
            dashTrailY[0]=birdY;
            dashFrames--;
        } else {
            // Fade trail out
            if(dashTrailCount>0) dashTrailCount--;
        }

        // Pipe speed
        float targetSpeed = slowmo ? BASE_PIPE_SPEED*0.45f : BASE_PIPE_SPEED;
        pipeSpeed += (targetSpeed - pipeSpeed)*0.08f; // smooth transition
        // Slow tint
        slowTint = slowmo ? std::min(slowTint+4, 50) : std::max(slowTint-4, 0);

        // Record safe position: not near walls, not overlapping any pipe
        {
            bool nearWall=(birdY<80.f||birdY>(float)(GROUND_Y-60));
            bool nearPipe=false;
            for(int i=0;i<PIPE_COUNT;i++){
                if(pipes[i].x < BIRD_X+BIRD_W/2+20 && pipes[i].x+PIPE_W > BIRD_X-BIRD_W/2-20){
                    nearPipe=true; break;
                }
            }
            if(!nearWall && !nearPipe) lastSafeY=birdY;
        }
        birdVY=std::min(birdVY+GRAVITY,MAX_FALL);
        birdY+=birdVY;
        if(g_tampered) birdY+=(float)((rand()%5)-2);
        birdAngle=std::max(-25.f,std::min(birdVY*3.5f,90.f));

        RECT bird={BIRD_X-BIRD_W/2+4,(int)(birdY-BIRD_H/2+4),
                   BIRD_X+BIRD_W/2-4,(int)(birdY+BIRD_H/2-4)};
        if(birdY+BIRD_H/2>=GROUND_Y||birdY-BIRD_H/2<=0){
            if(abilities[2].active>0){
                abilities[2].active=0;   // heart consumed
                birdY = lastSafeY;       // teleport to last safe position
                birdVY = 0.f;
                invincFrames = 90;       // 1.5s invincibility window
                playSound(SND_HIT,SND_HIT_LEN);
                return;
            } else if(invincFrames<=0){ die(); return; }
        }

        for(int i=0;i<PIPE_COUNT;i++){
            pipes[i].x-=pipeSpeed;
            RECT tp={(int)pipes[i].x,0,(int)pipes[i].x+PIPE_W,pipes[i].topH};
            RECT bp={(int)pipes[i].x,pipes[i].topH+PIPE_GAP,(int)pipes[i].x+PIPE_W,GROUND_Y};
            if(!ghost && (hitRect(bird,tp)||hitRect(bird,bp))){
                // Heart check
                if(abilities[2].active>0){
                    abilities[2].active=0;   // heart consumed
                    birdY = lastSafeY;       // teleport to last safe Y
                    birdVY = 0.f;
                    invincFrames = 90;       // 1.5s grace window
                    playSound(SND_HIT,SND_HIT_LEN);
                } else if(invincFrames<=0){ die(); return; }
            }
            if(!pipes[i].scored&&pipes[i].x+PIPE_W/2<BIRD_X){
                pipes[i].scored=true;
                if(!g_tampered){
                    score++;
                    if(score>best){best=score;setInteg(best);}
                    playSoundThread(SND_SCORE,SND_SCORE_LEN);
                }
            }
            if(pipes[i].x+PIPE_W<0){
                float mx=0;
                for(int j=0;j<PIPE_COUNT;j++)if(pipes[j].x>mx)mx=pipes[j].x;
                pipes[i].x=mx+PIPE_SPACE;
                int gap=g_tampered?40:GROUND_Y-PIPE_GAP-100;
                pipes[i].topH=80+rand()%gap;
                pipes[i].scored=false;
            }
        }
    }

    void die(){
        state=DEAD; deathTime=GetTickCount();
        playSound(SND_HIT,SND_HIT_LEN);
        playSoundThread(SND_DIE,SND_DIE_LEN);
        earnedCoins = g_tampered ? 0 : calcCoins(score);
        totalCoins += earnedCoins;
        if(totalCoins>999999) totalCoins=999999;
        saveData();
    }

    void saveData(){
        HKEY hk;
        if(RegCreateKeyExA(HKEY_CURRENT_USER,"Software\\WingIt47",0,NULL,0,
                           KEY_WRITE,NULL,&hk,NULL)==ERROR_SUCCESS){
            DWORD enc=encodeScore(best);
            RegSetValueExA(hk,"BestScore",0,REG_DWORD,(BYTE*)&enc,sizeof(DWORD));
            DWORD ec=encodeCoins(totalCoins);
            RegSetValueExA(hk,"TotalCoins",0,REG_DWORD,(BYTE*)&ec,sizeof(DWORD));
            // Save ability uses
            for(int i=0;i<ABILITY_COUNT;i++){
                char vname[32]; sprintf(vname,"AbilityUses%d",i);
                DWORD lv=(DWORD)abilities[i].uses;
                RegSetValueExA(hk,vname,0,REG_DWORD,(BYTE*)&lv,sizeof(DWORD));
            }
            RegCloseKey(hk);
        }
    }

    void loadData(){
        HKEY hk; best=0; totalCoins=0;
        for(int i=0;i<ABILITY_COUNT;i++) abilities[i].uses=0;
        if(RegOpenKeyExA(HKEY_CURRENT_USER,"Software\\WingIt47",0,KEY_READ,&hk)==ERROR_SUCCESS){
            DWORD v=0,sz=sizeof(DWORD),t=REG_DWORD;
            if(RegQueryValueExA(hk,"BestScore",NULL,&t,(BYTE*)&v,&sz)==ERROR_SUCCESS) best=decodeScore(v);
            sz=sizeof(DWORD); t=REG_DWORD; v=0;
            if(RegQueryValueExA(hk,"TotalCoins",NULL,&t,(BYTE*)&v,&sz)==ERROR_SUCCESS) totalCoins=decodeCoins(v);
            for(int i=0;i<ABILITY_COUNT;i++){
                char vname[32]; sprintf(vname,"AbilityUses%d",i);
                sz=sizeof(DWORD); t=REG_DWORD; v=0;
                if(RegQueryValueExA(hk,vname,NULL,&t,(BYTE*)&v,&sz)==ERROR_SUCCESS)
                    abilities[i].uses=(int)v;
            }
            RegCloseKey(hk);
        }
        setInteg(best);
    }

    // Shop logic
    void shopBuy(int qty=1){
        int idx=shopCursor;
        int space=ABILITY_MAX_STACK-abilities[idx].uses;
        if(space<=0) return;
        int actual=std::min(qty,space);        // can't exceed stack cap
        int cost=ABILITY_COST[idx]*actual;
        if(totalCoins<cost){
            // Buy as many as we can afford
            actual=totalCoins/ABILITY_COST[idx];
            if(actual<=0) return;
            cost=ABILITY_COST[idx]*actual;
        }
        totalCoins-=cost;
        abilities[idx].uses+=actual;
        saveData();
        playSound(SND_SCORE,SND_SCORE_LEN);
    }
} g;

// ====================== DRAW ===================================
HBITMAP hBuf; HDC hMemDC;

void fillRect(HDC dc,int x,int y,int w,int h,COLORREF c){
    HBRUSH b=CreateSolidBrush(c); RECT r={x,y,x+w,y+h};
    FillRect(dc,&r,b); DeleteObject(b);
}
void fillRectAlpha(HDC dc,int x,int y,int w,int h,COLORREF c,int alpha){
    // Blend a solid color over the backbuffer using a temp DC
    HDC tmp=CreateCompatibleDC(dc);
    HBITMAP bmp=CreateCompatibleBitmap(dc,w,h);
    SelectObject(tmp,bmp);
    HBRUSH b=CreateSolidBrush(c); RECT r={0,0,w,h};
    FillRect(tmp,&r,b); DeleteObject(b);
    BLENDFUNCTION bf={AC_SRC_OVER,0,(BYTE)alpha,0};
    AlphaBlend(dc,x,y,w,h,tmp,0,0,w,h,bf);
    DeleteDC(tmp); DeleteObject(bmp);
}
void txt(HDC dc,const char*s,int x,int y,int sz,COLORREF c,bool center=false){
    HFONT f=CreateFontA(sz,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
    HFONT old=(HFONT)SelectObject(dc,f);
    SetTextColor(dc,c); SetBkMode(dc,TRANSPARENT);
    if(center){SIZE sz2;GetTextExtentPoint32A(dc,s,(int)strlen(s),&sz2);x-=sz2.cx/2;}
    TextOutA(dc,x,y,s,(int)strlen(s)); SelectObject(dc,old); DeleteObject(f);
}
void drawBird(HDC dc,int cx,int cy,float angle,bool ghost,bool heart){
    XFORM xf; float r=angle*3.14159f/180.f;
    xf.eM11=cosf(r);xf.eM12=sinf(r);xf.eM21=-sinf(r);xf.eM22=cosf(r);
    xf.eDx=(float)cx;xf.eDy=(float)cy;
    int om=SetGraphicsMode(dc,GM_ADVANCED); SetWorldTransform(dc,&xf);
    auto el=[&](int x1,int y1,int x2,int y2,COLORREF f2,COLORREF s2,int sw=1){
        HBRUSH b=CreateSolidBrush(f2);HPEN p=CreatePen(PS_SOLID,sw,s2);
        SelectObject(dc,b);SelectObject(dc,p);
        Ellipse(dc,x1,y1,x2,y2);DeleteObject(b);DeleteObject(p);
    };
    COLORREF bodyCol = ghost ? RGB(100,180,255) : RGB(255,213,0);
    COLORREF bodyOut = ghost ? RGB(60,120,200)  : RGB(180,140,0);
    el(-BIRD_W/2,-BIRD_H/2,BIRD_W/2,BIRD_H/2,bodyCol,bodyOut,2);
    el(-10,2,10,14,ghost?RGB(80,140,220):RGB(255,170,0),ghost?RGB(60,100,180):RGB(160,110,0));
    el(6,-10,18,2,RGB(255,255,255),RGB(0,0,0));
    el(10,-8,16,-2,RGB(0,0,0),RGB(0,0,0));
    POINT bk[3]={{16,-3},{26,0},{16,5}};
    HBRUSH bb=CreateSolidBrush(ghost?RGB(60,120,200):RGB(255,100,0));
    SelectObject(dc,bb);SelectObject(dc,GetStockObject(NULL_PEN));
    Polygon(dc,bk,3);DeleteObject(bb);
    // Heart glow ring (red when heart ability active)
    if(heart){
        HPEN sp=CreatePen(PS_SOLID,3,RGB(255,60,100));
        SelectObject(dc,GetStockObject(NULL_BRUSH));SelectObject(dc,sp);
        Ellipse(dc,-BIRD_W/2-5,-BIRD_H/2-5,BIRD_W/2+5,BIRD_H/2+5);
        DeleteObject(sp);
    }
    XFORM id={1,0,0,1,0,0}; SetWorldTransform(dc,&id); SetGraphicsMode(dc,om);
}
void drawPipe(HDC dc,int x,int topH,bool ghost){
    int by=topH+PIPE_GAP,bh=GROUND_Y-by;
    COLORREF gc=ghost?RGB(160,220,140):RGB(80,180,60);
    COLORREF dc2=ghost?RGB(120,180,100):RGB(60,160,40);
    COLORREF lc=ghost?RGB(180,240,160):RGB(120,220,100);
    fillRect(dc,x,0,PIPE_W,topH,gc);
    fillRect(dc,x-5,topH-18,PIPE_W+10,18,dc2);
    fillRect(dc,x+6,0,10,topH-18,lc);
    fillRect(dc,x,by,PIPE_W,bh,gc);
    fillRect(dc,x-5,by,PIPE_W+10,18,dc2);
    fillRect(dc,x+6,by+18,10,bh-18,lc);
}

// ── Ability icon drawing functions (100% original GDI art) ──────────────────
static void drawIconGhost(HDC dc, int x, int y, COLORREF col){
    // Round head top (ellipse top half)
    HBRUSH b=CreateSolidBrush(col); HPEN p=CreatePen(PS_SOLID,1,col);
    SelectObject(dc,b); SelectObject(dc,p);
    Ellipse(dc,x+4,y+2,x+28,y+24); // head circle
    // Rectangular body below center
    RECT body={x+4,y+13,x+28,y+30}; FillRect(dc,&body,b);
    DeleteObject(b); DeleteObject(p);
    // Wavy bottom — 3 bumps using small ellipses cut from below
    HBRUSH bg2=CreateSolidBrush(RGB(0,0,0));
    SelectObject(dc,bg2); SelectObject(dc,GetStockObject(NULL_PEN));
    Ellipse(dc,x+2, y+24,x+12,y+34);
    Ellipse(dc,x+11,y+24,x+21,y+34);
    Ellipse(dc,x+20,y+24,x+30,y+34);
    DeleteObject(bg2);
    // Eyes (two dark ovals)
    HBRUSH eb=CreateSolidBrush(RGB(20,20,40));
    SelectObject(dc,eb); SelectObject(dc,GetStockObject(NULL_PEN));
    Ellipse(dc,x+8, y+9,x+14,y+16);
    Ellipse(dc,x+18,y+9,x+24,y+16);
    DeleteObject(eb);
    // Eye shine
    HBRUSH sh=CreateSolidBrush(RGB(255,255,255));
    SelectObject(dc,sh);
    Ellipse(dc,x+9, y+10,x+12,y+13);
    Ellipse(dc,x+19,y+10,x+22,y+13);
    DeleteObject(sh);
}

static void drawIconSlowMo(HDC dc, int x, int y, COLORREF col){
    // Clock circle
    HPEN p=CreatePen(PS_SOLID,2,col);
    SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,p);
    Ellipse(dc,x+3,y+2,x+29,y+30);
    DeleteObject(p);
    // Clock hands from center (x+16,y+16)
    HPEN hp=CreatePen(PS_SOLID,2,col);
    SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,hp);
    MoveToEx(dc,x+16,y+16,NULL); LineTo(dc,x+16,y+7);  // 12 o'clock (hour)
    MoveToEx(dc,x+16,y+16,NULL); LineTo(dc,x+23,y+20); // 3 o'clock (minute)
    DeleteObject(hp);
    // Center dot
    HBRUSH cb=CreateSolidBrush(col);
    SelectObject(dc,cb); SelectObject(dc,GetStockObject(NULL_PEN));
    Ellipse(dc,x+14,y+14,x+18,y+18);
    DeleteObject(cb);
    // Pause bars on right side (slow = pause symbol hint)
    HBRUSH pb=CreateSolidBrush(col);
    SelectObject(dc,pb);
    RECT r1={x+22,y+5,x+25,y+14}; FillRect(dc,&r1,pb);
    RECT r2={x+27,y+5,x+30,y+14}; FillRect(dc,&r2,pb);
    DeleteObject(pb);
}

static void drawIconHeart(HDC dc, int x, int y, COLORREF col){
    // Heart shape: two circles + a triangle bottom
    HBRUSH b=CreateSolidBrush(col); HPEN p=CreatePen(PS_SOLID,1,col);
    SelectObject(dc,b); SelectObject(dc,p);
    Ellipse(dc,x+4, y+6, x+17, y+19);   // left lobe
    Ellipse(dc,x+15,y+6, x+28, y+19);   // right lobe
    // Triangle bottom (fill the lower V)
    POINT tri[3]={{x+4,y+13},{x+28,y+13},{x+16,y+30}};
    Polygon(dc,tri,3);
    DeleteObject(b); DeleteObject(p);
    // Inner shine
    HPEN sp=CreatePen(PS_SOLID,2,RGB(255,255,255));
    SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,sp);
    MoveToEx(dc,x+9, y+9,NULL); LineTo(dc,x+13,y+8);
    DeleteObject(sp);
}

static void drawIconDash(HDC dc, int x, int y, COLORREF col){
    // Arrow pointing right = dash
    HBRUSH b=CreateSolidBrush(col); HPEN p=CreatePen(PS_SOLID,1,col);
    SelectObject(dc,b); SelectObject(dc,p);
    // Arrow head
    POINT arr[3]={{x+28,y+16},{x+16,y+6},{x+16,y+26}};
    Polygon(dc,arr,3);
    DeleteObject(b); DeleteObject(p);
    // Shaft lines (3 speed lines)
    HPEN lp=CreatePen(PS_SOLID,2,col);
    SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,lp);
    MoveToEx(dc,x+4, y+12,NULL); LineTo(dc,x+16,y+12);
    MoveToEx(dc,x+4, y+16,NULL); LineTo(dc,x+16,y+16);
    MoveToEx(dc,x+4, y+20,NULL); LineTo(dc,x+16,y+20);
    DeleteObject(lp);
}

// Draw an ability button in HUD (vertical stack, right side, inside play area)
void drawAbilityHUD(HDC dc, int idx, int x, int y){
    Ability& ab = g.abilities[idx];
    bool active=(ab.active>0);
    bool onCD=(ab.cooldown>0);
    bool hasUses=(ab.uses>0);

    // Background
    COLORREF bg = active ? RGB(30,50,70) : onCD ? RGB(35,25,10) : hasUses ? RGB(25,25,50) : RGB(15,15,15);
    fillRect(dc,x,y,46,46,bg);

    // Border
    COLORREF bord = active?RGB(0,220,255): onCD?RGB(200,130,0): hasUses?RGB(120,120,200):RGB(40,40,40);
    HPEN lp=CreatePen(PS_SOLID,2,bord);
    SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,lp);
    Rectangle(dc,x,y,x+46,y+46); DeleteObject(lp);

    // Icon color
    COLORREF ic = active?RGB(0,220,255): onCD?RGB(160,100,0): hasUses?RGB(180,180,240):RGB(50,50,50);

    // Draw icon
    int ox=x+7, oy=y+6;
    if(idx==0) drawIconGhost (dc,ox,oy,ic);
    if(idx==1) drawIconSlowMo(dc,ox,oy,ic);
    if(idx==2) drawIconHeart (dc,ox,oy,ic);
    if(idx==3) drawIconDash  (dc,ox,oy,ic);

    // Cooldown overlay: dim + show seconds remaining inside button
    if(onCD){
        fillRectAlpha(dc,x,y,46,46,RGB(0,0,0),120);
        char cds[8]; sprintf(cds,"%.1fs",(float)ab.cooldown/60.f);
        txt(dc,cds,x+23,y+16,13,RGB(255,180,0),true);
    }

    // Active duration arc (thin progress bar along bottom edge)
    if(active && idx!=2){
        int bw=(int)(46.f*ab.active/ABILITY_DUR[idx]);
        fillRect(dc,x,y+40,46,6,RGB(20,20,20));
        fillRect(dc,x,y+40,bw,6,RGB(0,230,100));
        // Remaining seconds text in button
        char ds[8]; sprintf(ds,"%.1fs",(float)ab.active/60.f);
        txt(dc,ds,x+23,y+16,11,RGB(0,230,180),true);
    }

    // Heart active: show heart symbol pulsing
    if(active && idx==2){
        txt(dc,"LIVE",x+23,y+18,10,RGB(255,80,100),true);
    }

    // Uses counter — top-left badge
    if(hasUses || active){
        char uc[6]; sprintf(uc,"x%d",ab.uses+(active&&idx!=3?1:0));
        txt(dc,uc,x+2,y+2,10,RGB(255,210,60));
    }

    // Key hint bottom-right corner
    char kh[4]; sprintf(kh,"%d",idx+1);
    txt(dc,kh,x+35,y+34,10,RGB(80,80,80));
}

void drawScene(HDC dc){
    // Sky
    fillRect(dc,0,0,WW,GROUND_Y/2,RGB(112,197,206));
    fillRect(dc,0,GROUND_Y/2,WW,GROUND_Y-GROUND_Y/2,RGB(140,210,220));
    // Clouds
    HBRUSH cb=CreateSolidBrush(RGB(255,255,255));
    SelectObject(dc,cb);SelectObject(dc,GetStockObject(NULL_PEN));
    Ellipse(dc,40,60,110,100);Ellipse(dc,70,45,130,90);Ellipse(dc,100,60,160,100);
    Ellipse(dc,200,110,260,145);Ellipse(dc,225,98,280,138);Ellipse(dc,255,110,310,145);
    DeleteObject(cb);

    bool ghost=(g.abilities[0].active>0);
    bool heart=(g.abilities[2].active>0);
    // MENU/OPTIONS/PAUSE use bg anim; DEAD uses frozen real game state
    if(g.state==MENU||g.state==OPTIONS){
        for(int i=0;i<PIPE_COUNT;i++) drawPipe(dc,(int)g_bg.pipeX[i],g_bg.pipeTopH[i],false);
        fillRect(dc,0,GROUND_Y,WW,GROUND_H,RGB(222,178,100));
        fillRect(dc,0,GROUND_Y,WW,6,RGB(120,200,80));
        drawBird(dc,BIRD_X,(int)g_bg.birdY,0.f,false,false);
    } else if(g.state==DEAD||g.state==PAUSE){
        // Frozen real game scene
        for(int i=0;i<PIPE_COUNT;i++) drawPipe(dc,(int)g.pipes[i].x,g.pipes[i].topH,false);
        fillRect(dc,0,GROUND_Y,WW,GROUND_H,RGB(222,178,100));
        fillRect(dc,0,GROUND_Y,WW,6,RGB(120,200,80));
        drawBird(dc,BIRD_X,(int)g.birdY,g.birdAngle,false,false);
    } else {
        for(int i=0;i<PIPE_COUNT;i++) drawPipe(dc,(int)g.pipes[i].x,g.pipes[i].topH,ghost);
        fillRect(dc,0,GROUND_Y,WW,GROUND_H,RGB(222,178,100));
        fillRect(dc,0,GROUND_Y,WW,6,RGB(120,200,80));
        // Dash trail — ghost copies fading behind bird
        if(g.dashTrailCount>0){
            for(int t=g.dashTrailCount-1;t>=0;t--){
                // alpha fades: closest copy most opaque
                int alpha = 160 - t*28;
                if(alpha<20) alpha=20;
                // Draw a cyan streak rect at each trail position (simple motion blur)
                COLORREF tc=RGB(0,200,255);
                fillRectAlpha(dc,
                    (int)g.dashTrailX[t]-BIRD_W/2+4,
                    (int)g.dashTrailY[t]-BIRD_H/2+4,
                    BIRD_W-8, BIRD_H-8,
                    tc, alpha);
                // Also draw a faint ghost bird silhouette
                drawBird(dc,(int)g.dashTrailX[t],(int)g.dashTrailY[t],
                         g.birdAngle*0.3f, true, false);
            }
            // Speed lines — horizontal streaks from bird leftward
            int bx=BIRD_X-BIRD_W/2;
            int by=(int)g.birdY;
            HPEN sp1=CreatePen(PS_SOLID,2,RGB(0,200,255));
            HPEN sp2=CreatePen(PS_SOLID,1,RGB(100,230,255));
            SelectObject(dc,GetStockObject(NULL_BRUSH));
            SelectObject(dc,sp1);
            MoveToEx(dc,bx-10,by-6,NULL); LineTo(dc,bx-10-30-(g.dashFrames*3),by-6);
            MoveToEx(dc,bx-10,by+6,NULL); LineTo(dc,bx-10-30-(g.dashFrames*3),by+6);
            SelectObject(dc,sp2);
            MoveToEx(dc,bx-5,by,NULL);   LineTo(dc,bx-5 -20-(g.dashFrames*2),by);
            DeleteObject(sp1); DeleteObject(sp2);
        }
        // Flash bird during post-heart invincibility
        bool showBird = (g.invincFrames<=0) || ((g.invincFrames/6)%2==0);
        if(showBird)
            drawBird(dc,BIRD_X,(int)g.birdY,g.birdAngle,ghost,heart);
        // Invincibility countdown ring around bird
        if(g.invincFrames>0){
            float prog = (float)g.invincFrames/90.f; // 1.0 → 0.0
            int   rad  = (int)(20 + 14*(1.f-prog));  // ring grows as it expires
            HPEN  rp   = CreatePen(PS_SOLID,2,RGB(255,(int)(80+150*prog),(int)(80+150*prog)));
            SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,rp);
            Ellipse(dc,BIRD_X-rad,(int)g.birdY-rad,BIRD_X+rad,(int)g.birdY+rad);
            DeleteObject(rp);
        }
    }

    // Slow mo tint
    if(g.slowTint>0) fillRectAlpha(dc,0,0,WW,GROUND_Y,RGB(100,140,255),g.slowTint);

    char buf[64];
    if(g.state==PLAYING){
        // Score
        sprintf(buf,"%d",g.score);
        txt(dc,buf,WW/2+2,32,48,RGB(0,0,0),true);
        txt(dc,buf,WW/2,30,48,RGB(255,255,255),true);
        // Coins this run
        sprintf(buf,"[%d pts]",g.calcCoins(g.score));
        txt(dc,buf,WW/2,82,13,RGB(255,230,100),true);
        // Ability HUD — right side, inside play area, vertical stack
        int hx=WW-54; // 4px from right edge
        int hy=100;   // start y
        for(int i=0;i<ABILITY_COUNT;i++) drawAbilityHUD(dc,i,hx,hy+i*52);
    }

    if(g.state==MENU){
        // Title
        txt(dc,"WING IT",WW/2+2,82,34,RGB(0,0,0),true);
        txt(dc,"WING IT",WW/2,80,34,RGB(255,230,0),true);
        sprintf(buf,"BEST: %d",g.best);        txt(dc,buf,WW/2,122,15,RGB(255,255,255),true);
        sprintf(buf,"COINS: %d",g.totalCoins); txt(dc,buf,WW/2,142,14,RGB(255,220,80),true);
        // Menu card background
        fillRectAlpha(dc,WW/2-110,175,220,195,RGB(10,15,30),210);
        HPEN mp=CreatePen(PS_SOLID,2,RGB(70,80,110));
        SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,mp);
        Rectangle(dc,WW/2-110,175,WW/2+110,370); DeleteObject(mp);
        // PLAY button
        fillRect(dc,WW/2-88,188,176,46,RGB(30,80,30));
        HPEN playp=CreatePen(PS_SOLID,2,RGB(80,200,80));
        SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,playp);
        Rectangle(dc,WW/2-88,188,WW/2+88,234); DeleteObject(playp);
        txt(dc,"PLAY",WW/2,194,20,RGB(130,255,130),true);
        txt(dc,"SPACE or Click",WW/2,216,10,RGB(70,150,70),true);
        // SHOP button
        fillRect(dc,WW/2-88,244,176,40,RGB(20,25,55));
        HPEN shopp=CreatePen(PS_SOLID,1,RGB(60,75,140));
        SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,shopp);
        Rectangle(dc,WW/2-88,244,WW/2+88,284); DeleteObject(shopp);
        txt(dc,"SHOP  [S]",WW/2,254,15,RGB(150,170,255),true);
        // OPTIONS button
        fillRect(dc,WW/2-88,294,176,40,RGB(20,25,55));
        HPEN optp=CreatePen(PS_SOLID,1,RGB(60,75,140));
        SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,optp);
        Rectangle(dc,WW/2-88,294,WW/2+88,334); DeleteObject(optp);
        txt(dc,"OPTIONS  [ESC]",WW/2,304,15,RGB(150,200,200),true);
        txt(dc,"(c) 2026 Ayoub Assouar",WW/2,356,11,RGB(130,160,130),true);
    }

    if(g.state==DEAD){
        // Dim overlay over the live bg
        fillRectAlpha(dc,0,0,WW,WH,RGB(0,0,0),100);
        // Death card
        fillRect(dc,WW/2-120,140,240,220,RGB(222,178,100));
        HPEN pp=CreatePen(PS_SOLID,3,RGB(120,80,40));
        SelectObject(dc,GetStockObject(NULL_BRUSH));SelectObject(dc,pp);
        Rectangle(dc,WW/2-120,140,WW/2+120,360); DeleteObject(pp);
        txt(dc,"GAME OVER",WW/2,152,24,RGB(200,0,0),true);
        // Divider
        HPEN dp=CreatePen(PS_SOLID,1,RGB(150,110,60));
        SelectObject(dc,GetStockObject(NULL_BRUSH));SelectObject(dc,dp);
        MoveToEx(dc,WW/2-100,188,NULL);LineTo(dc,WW/2+100,188); DeleteObject(dp);
        sprintf(buf,"Score:  %d",g.score);  txt(dc,buf,WW/2,196,17,RGB(0,0,0),true);
        sprintf(buf,"Best:   %d",g.best);   txt(dc,buf,WW/2,218,17,RGB(0,0,0),true);
        // Points earned highlight
        fillRect(dc,WW/2-90,244,180,36,RGB(255,220,60));
        sprintf(buf,"+%d COINS",g.earnedCoins); txt(dc,buf,WW/2,252,20,RGB(80,40,0),true);
        sprintf(buf,"Total: %d",g.totalCoins); txt(dc,buf,WW/2,292,14,RGB(80,40,0),true);
        txt(dc,"SPACE / Click to continue",WW/2,326,12,RGB(255,255,255),true);
    }

    if(g.state==PAUSE){
        // Dark overlay over frozen game
        fillRectAlpha(dc,0,0,WW,WH,RGB(0,0,0),140);
        // Pause card
        fillRect(dc,WW/2-110,180,220,160,RGB(20,25,45));
        HPEN pcp=CreatePen(PS_SOLID,2,RGB(80,100,160));
        SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,pcp);
        Rectangle(dc,WW/2-110,180,WW/2+110,340); DeleteObject(pcp);
        txt(dc,"PAUSED",WW/2+2,192,26,RGB(0,0,0),true);
        txt(dc,"PAUSED",WW/2,190,26,RGB(255,220,50),true);
        // Resume button
        fillRect(dc,WW/2-84,228,168,38,RGB(30,70,30));
        HPEN rp=CreatePen(PS_SOLID,2,RGB(80,180,80));
        SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,rp);
        Rectangle(dc,WW/2-84,228,WW/2+84,266); DeleteObject(rp);
        txt(dc,"RESUME  [ESC]",WW/2,237,15,RGB(130,255,130),true);
        // Quit to menu button
        fillRect(dc,WW/2-84,278,168,38,RGB(50,20,20));
        HPEN qp2=CreatePen(PS_SOLID,1,RGB(140,60,60));
        SelectObject(dc,GetStockObject(NULL_BRUSH)); SelectObject(dc,qp2);
        Rectangle(dc,WW/2-84,278,WW/2+84,316); DeleteObject(qp2);
        txt(dc,"QUIT RUN  [Q]",WW/2,287,15,RGB(255,120,120),true);
    }

    if(g.state==SHOP){
        // Shop overlay
        fillRect(dc,0,0,WW,WH,RGB(10,15,30));
        txt(dc,"SHOP",WW/2,18,28,RGB(255,220,50),true);
        sprintf(buf,"Coins: %d",g.totalCoins); txt(dc,buf,WW/2,50,15,RGB(255,220,80),true);

        // Scrollable card list — clip to [72..WH-52]
        int listTop=72, listBot=WH-52;
        int cardH=138; // height per card including gap
        int CARD_AREA_H = ABILITY_COUNT*cardH;
        // Clamp scroll
        int maxScroll=std::max(0,CARD_AREA_H-(listBot-listTop));
        if(g.shopScroll>maxScroll) g.shopScroll=maxScroll;
        if(g.shopScroll<0) g.shopScroll=0;

        // Draw clip region manually: only render cards whose y falls in [listTop,listBot]
        for(int i=0;i<ABILITY_COUNT;i++){
            int iy = listTop + i*cardH - g.shopScroll;
            if(iy+cardH < listTop || iy > listBot) continue; // out of view

            bool sel=(g.shopCursor==i);
            COLORREF cardBg=sel?RGB(40,50,80):RGB(20,25,45);
            // Clamp draw to visible area (simple: just draw, GDI clips to window anyway)
            fillRect(dc,20,iy,358,cardH-10,cardBg);
            HPEN cp=CreatePen(PS_SOLID,sel?3:1,sel?RGB(255,220,50):RGB(60,70,100));
            SelectObject(dc,GetStockObject(NULL_BRUSH));SelectObject(dc,cp);
            Rectangle(dc,20,iy,378,iy+cardH-10); DeleteObject(cp);

            int uses=g.abilities[i].uses;
            bool maxed=(uses>=ABILITY_MAX_STACK);
            bool canAfford1=(g.totalCoins>=ABILITY_COST[i]);
            bool canAfford5=(g.totalCoins>=ABILITY_COST[i]*5);

            txt(dc,ABILITY_NAMES[i],36,iy+8, 17,RGB(200,220,255));
            txt(dc,ABILITY_DESC[i], 36,iy+28,12,RGB(140,160,200));
            sprintf(buf,"Owned: %d / %d",uses,ABILITY_MAX_STACK);
            txt(dc,buf,36,iy+48,13,uses>0?RGB(255,220,80):RGB(120,120,120));

            if(maxed){
                txt(dc,"FULL (5/5)",36,iy+70,13,RGB(180,180,180));
            } else {
                // Buy x1 button
                sprintf(buf,"[SPACE] x1  %d coins",ABILITY_COST[i]);
                txt(dc,buf,36,iy+70,12,canAfford1?RGB(100,255,100):RGB(255,100,100));
                // Buy x5 button
                int cost5=ABILITY_COST[i]*std::min(5,ABILITY_MAX_STACK-uses);
                sprintf(buf,"[E]     x5  %d coins",cost5);
                txt(dc,buf,36,iy+88,12,canAfford5?RGB(100,220,255):RGB(160,100,100));
            }
            // Duration info
            if(i==0||i==1){ sprintf(buf,"Lasts: %ds",ABILITY_DUR[i]/60); txt(dc,buf,36,iy+108,11,RGB(100,180,140)); }
            else if(i==2){ txt(dc,"Revive once on hit",36,iy+108,11,RGB(100,180,140)); }
            else if(i==3){ txt(dc,"Instant dash, 1.5s CD",36,iy+108,11,RGB(100,180,140)); }
        }

        // Fade top/bottom edges to hint scrollability
        if(g.shopScroll>0)      fillRectAlpha(dc,0,listTop,WW,30,RGB(10,15,30),180);
        if(g.shopScroll<maxScroll) fillRectAlpha(dc,0,listBot-30,WW,30,RGB(10,15,30),180);

        // Scroll indicator bar
        if(maxScroll>0){
            int barH=(int)((float)(listBot-listTop)*((listBot-listTop)/(float)CARD_AREA_H));
            int barY=listTop+(int)((float)(listBot-listTop-barH)*((float)g.shopScroll/maxScroll));
            fillRect(dc,WW-6,listTop,4,listBot-listTop,RGB(30,35,55));
            fillRect(dc,WW-6,barY,4,barH,RGB(100,120,200));
        }

        txt(dc,"[W/S] Select   [SPACE] Buy x1   [E] Buy x5   [ESC] Back",WW/2,WH-38,10,RGB(120,140,180),true);
    }

    if(g.state==OPTIONS){
        // Dark overlay over the bg
        fillRectAlpha(dc,0,0,WW,WH,RGB(0,0,0),160);
        txt(dc,"OPTIONS",WW/2,60,28,RGB(255,220,50),true);

        // FPS row
        bool fsel=(g.optCursor==0);
        int oy=180;
        fillRect(dc,WW/2-130,oy-4,260,36,fsel?RGB(40,50,80):RGB(20,25,45));
        HPEN fp=CreatePen(PS_SOLID,fsel?2:1,fsel?RGB(255,220,50):RGB(60,70,100));
        SelectObject(dc,GetStockObject(NULL_BRUSH));SelectObject(dc,fp);
        Rectangle(dc,WW/2-130,oy-4,WW/2+130,oy+32); DeleteObject(fp);
        txt(dc,"MAX FPS",WW/2-120,oy+4,16,RGB(200,220,255));
        // Left arrow
        txt(dc,"<",WW/2+20,oy+4,16,RGB(180,180,180));
        txt(dc,FPS_LABELS[g_fpsChoice],WW/2+44,oy+4,16,RGB(255,220,50));
        txt(dc,">",WW/2+80,oy+4,16,RGB(180,180,180));

        txt(dc,"[LEFT/RIGHT] Change    [ESC] Back",WW/2,480,12,RGB(120,140,180),true);

        // Quit button at bottom
        fillRect(dc,WW/2-70,510,140,32,RGB(60,20,20));
        HPEN qp=CreatePen(PS_SOLID,1,RGB(180,60,60));
        SelectObject(dc,GetStockObject(NULL_BRUSH));SelectObject(dc,qp);
        Rectangle(dc,WW/2-70,510,WW/2+70,542); DeleteObject(qp);
        txt(dc,"QUIT GAME",WW/2,518,14,RGB(255,100,100),true);
        txt(dc,"[ENTER] to confirm quit",WW/2,548,11,RGB(150,80,80),true);
    }
}

// ====================== ICON ===================================
static HICON loadEmbeddedIcon(int size){
    char tmp2[MAX_PATH],path[MAX_PATH];
    GetTempPathA(MAX_PATH,tmp2); sprintf(path,"%swingit47.ico",tmp2);
    HANDLE hf=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    if(hf==INVALID_HANDLE_VALUE)return NULL;
    DWORD w; WriteFile(hf,ICO_DATA,ICO_DATA_LEN,&w,NULL); CloseHandle(hf);
    HICON ico=(HICON)LoadImageA(NULL,path,IMAGE_ICON,size,size,LR_LOADFROMFILE);
    DeleteFileA(path); return ico;
}

// ====================== INPUT ==================================
LRESULT CALLBACK WndProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_KEYDOWN:
        if(g.state==OPTIONS){
            if(wp==VK_LEFT||wp=='A'){ g_fpsChoice=(g_fpsChoice+2)%3; saveOptions(); }
            if(wp==VK_RIGHT||wp=='D'){ g_fpsChoice=(g_fpsChoice+1)%3; saveOptions(); }
            if(wp==VK_ESCAPE||wp=='Q'){ g.state=g.prevState; }
        } else if(g.state==SHOP){
            if(wp==VK_UP||wp=='W'){
                g.shopCursor=(g.shopCursor-1+ABILITY_COUNT)%ABILITY_COUNT;
                // Auto-scroll to keep selected card visible
                int cardTop=72+g.shopCursor*138-g.shopScroll;
                if(cardTop<72) g.shopScroll-=(72-cardTop)+4;
            }
            if(wp==VK_DOWN||wp=='S'){
                g.shopCursor=(g.shopCursor+1)%ABILITY_COUNT;
                int cardBot=72+(g.shopCursor+1)*138-g.shopScroll;
                if(cardBot>WH-52) g.shopScroll+=(cardBot-(WH-52))+4;
            }
            if(wp==VK_SPACE||wp==VK_RETURN) g.shopBuy(1);
            if(wp=='E') g.shopBuy(5);
            if(wp==VK_ESCAPE){ g.state=MENU; }
        } else if(g.state==MENU){
            if(wp==VK_SPACE||wp==VK_RETURN) g.startGame();
            if(wp=='S'){ g.state=SHOP; g.shopCursor=0; }
            if(wp==VK_ESCAPE){ g.prevState=MENU; g.state=OPTIONS; g.optCursor=0; }
        } else if(g.state==PAUSE){
            if(wp==VK_ESCAPE||wp==VK_SPACE) g.state=PLAYING; // resume
            if(wp=='Q'||wp==VK_RETURN){
                // Quit run → back to menu with fresh bg
                g_bg.init(); g.state=MENU;
            }
        } else {
            if(wp==VK_SPACE) g.jump();
            if(wp=='1') g.activateAbility(0);
            if(wp=='2') g.activateAbility(1);
            if(wp=='3') g.activateAbility(2);
            if(wp=='4') g.activateAbility(3);
            if(wp==VK_ESCAPE&&g.state==PLAYING){ g.state=PAUSE; } // pause
            if(wp==VK_ESCAPE&&g.state==DEAD){}  // ignore ESC on death screen
        }
        break;
    case WM_MOUSEWHEEL:
        if(g.state==SHOP){
            int delta=GET_WHEEL_DELTA_WPARAM(wp);
            g.shopScroll += (delta<0)?30:-30;
        }
        break;
    case WM_LBUTTONDOWN:{
        int mx=LOWORD(lp), my=HIWORD(lp);
        if(g.state==MENU){
            // PLAY button: x[112..288] y[188..234]
            if(mx>=112&&mx<=288&&my>=188&&my<=234){ g.startGame(); break; }
            // SHOP button: y[244..284]
            if(mx>=112&&mx<=288&&my>=244&&my<=284){ g.state=SHOP; g.shopCursor=0; break; }
            // OPTIONS button: y[294..334]
            if(mx>=112&&mx<=288&&my>=294&&my<=334){ g.prevState=MENU; g.state=OPTIONS; g.optCursor=0; break; }
            // Click anywhere else on menu → play
            g.startGame();
        } else if(g.state==PAUSE){
            // RESUME button: y[228..266]
            if(mx>=116&&mx<=284&&my>=228&&my<=266){ g.state=PLAYING; break; }
            // QUIT RUN button: y[278..316]
            if(mx>=116&&mx<=284&&my>=278&&my<=316){ g_bg.init(); g.state=MENU; break; }
        } else if(g.state!=SHOP){
            g.jump();
        }
        break;}
    case WM_DESTROY: PostQuitMessage(0); break;
    case WM_PAINT:{
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hw,&ps);
        BitBlt(hdc,0,0,WW,WH,hMemDC,0,0,SRCCOPY);
        EndPaint(hw,&ps); break;
    }
    default: return DefWindowProcA(hw,msg,wp,lp);
    }
    return 0;
}

// ====================== WINMAIN ================================
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE,LPSTR,int){
    srand((unsigned)time(NULL));
    initTiming();
    if(debuggerPresent()) g_tampered=true;

    HICON hBig=loadEmbeddedIcon(48),hSm=loadEmbeddedIcon(16);
    WNDCLASSA wc={};
    wc.lpfnWndProc=WndProc; wc.hInstance=hInst;
    wc.lpszClassName="WingIt47"; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); wc.hIcon=hBig;
    RegisterClassA(&wc);
    RECT wr={0,0,WW,WH};
    AdjustWindowRect(&wr,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);
    HWND hw=CreateWindowExA(0,"WingIt47","Wing It v6.0",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,wr.right-wr.left,wr.bottom-wr.top,
        NULL,NULL,hInst,NULL);
    if(hBig)SendMessage(hw,WM_SETICON,ICON_BIG,(LPARAM)hBig);
    if(hSm) SendMessage(hw,WM_SETICON,ICON_SMALL,(LPARAM)hSm);
    HDC hdc=GetDC(hw);
    hBuf=CreateCompatibleBitmap(hdc,WW,WH);
    hMemDC=CreateCompatibleDC(hdc); SelectObject(hMemDC,hBuf);
    ReleaseDC(hw,hdc);
    loadOptions(); g.loadData(); g.init();
    ShowWindow(hw,SW_SHOW); UpdateWindow(hw);

    MSG msg2={};
    LARGE_INTEGER qpf,qpc;
    QueryPerformanceFrequency(&qpf); QueryPerformanceCounter(&qpc);
    // Physics fixed at 60 Hz — render at chosen FPS cap
    LONGLONG physStep = qpf.QuadPart/60;       // always 60 physics ticks/sec
    LONGLONG renderStep = qpf.QuadPart/FPS_VALUES[g_fpsChoice];
    LONGLONG nextPhys   = qpc.QuadPart + physStep;
    LONGLONG nextRender = qpc.QuadPart + renderStep;
    timeBeginPeriod(1);
    while(true){
        while(PeekMessageA(&msg2,NULL,0,0,PM_REMOVE)){
            if(msg2.message==WM_QUIT) goto done;
            TranslateMessage(&msg2); DispatchMessageA(&msg2);
        }
        if(g.state==PLAYING&&!g_tampered&&debuggerPresent()) g_tampered=true;
        QueryPerformanceCounter(&qpc);
        // Physics tick — always 60/s, drives game speed
        if(qpc.QuadPart>=nextPhys){
            if(qpc.QuadPart>nextPhys+physStep*4) nextPhys=qpc.QuadPart;
            nextPhys+=physStep;
            g.update();
        }
        // Render tick — driven by FPS cap setting
        renderStep=qpf.QuadPart/FPS_VALUES[g_fpsChoice]; // pick up setting changes live
        if(qpc.QuadPart>=nextRender){
            if(qpc.QuadPart>nextRender+renderStep*4) nextRender=qpc.QuadPart;
            nextRender+=renderStep;
            drawScene(hMemDC);
            HDC h2=GetDC(hw); BitBlt(h2,0,0,WW,WH,hMemDC,0,0,SRCCOPY); ReleaseDC(hw,h2);
        }
        // Sleep until nearest upcoming tick
        QueryPerformanceCounter(&qpc);
        LONGLONG nextTick=nextPhys<nextRender?nextPhys:nextRender;
        LONGLONG msLeft=(nextTick-qpc.QuadPart)*1000/qpf.QuadPart;
        if(msLeft>1) Sleep((DWORD)(msLeft-1));
    }
    timeEndPeriod(1);
done:
    DeleteDC(hMemDC); DeleteObject(hBuf);
    return 0;
}

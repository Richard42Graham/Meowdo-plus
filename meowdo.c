/*
 * meowdo  ---  a cute bongo cat todo list ---
 *
 * compile:  gcc -O2 -o meowdo meowdo.c -lncursesw       (all distros, wide-char)
 * arch:     sudo pacman -S ncurses
 * debian:   sudo apt install libncursesw5-dev
 *
 * ------ keys ------------------------------------------------------------------------------------------------------------------------------------------
 *  j/k  ------            navigate
 *  g / G              jump top / bottom
 *  PgUp / PgDn        page up / down
 *  n                  new task
 *  e                  edit selected task
 *  Space              toggle done
 *  p                  toggle pin  ---
 *  t                  set / clear tag
 *  d                  delete selected task
 *  D                  delete ALL tasks
 *  /                  search
 *  Esc                clear search + filter
 *  1-6                filter by tag
 *  0                  show all
 *  q                  quit
 * ------------------------------------------------------------------------------------------------------------------------------------------------------------------
 *
 * data: ~/.local/share/meowdo/meowdo.json
 * json: {"version":1,"tasks":[{"pinned","done","tag","text","desc","created","done_at","sched_at"}],
 *        "notes":[{"text","created"}]}
 * (legacy todos.txt pipe-format is auto-imported on first run)
 * sched_at: tasks with sched_at > now are hidden until then (SCHEDULED tab)
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include <ncurses.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <wchar.h>
#include <strings.h>

/* ------ constants ------ */
#define MAX_TODOS  1024
#define MAX_LINE    256
#define MAX_TAG      32
#define MAX_UTAGS    64
#define LIST_Y0       4
#define BC_H         10
#define MAX_DESC    4096
#define MAX_NOTE    4096
#define MAX_NOTES    512

/* multiline editor limits (chars per line x lines) */
#define ML_LINES      8
#define ML_COLS      64

/* layout breakpoints */
#define MIN_COLS_FULL    80   /* below this: hide right pane */
#define MIN_COLS_COMPACT 20   /* below this: too small overlay */
#define MIN_ROWS         8    /* below this: too small overlay */

typedef enum { LAYOUT_FULL, LAYOUT_COMPACT, LAYOUT_TOOSMALL } Layout;

/* ------ colour pairs ------ */
enum {
    C_BORDER=1, C_TOPBAR=2, C_SEL=3,    C_DONE=4,  C_PEND=5,
    C_BONGO=6,  C_SBAR=7,   C_HDR=8,    C_GREEN=9,  C_PIN=10,
    C_TAG1=11,  C_TAG2=12,  C_TAG3=13,  C_TAG4=14,
    C_SEARCH=15,C_BEAT=16,  C_TITLE=17,
    C_TAG5=20,  C_TAG6=21,  C_TAG7=22,  C_TAG8=23,
};

/* ── UTF-8 capability flag (set at startup) ── */
static bool g_utf8 = 0;

/* ── bongo art: braille (UTF-8) ── */
static const char *BONGO_UTF8[BC_H] = {
    "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa2\x80\xe2\xa3\xbf\xe2\xa1\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80",
    "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa2\x80\xe2\xa3\xbe\xe2\xa3\xbf\xe2\xa1\x87\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa2\x80\xe2\xa3\xbc\xe2\xa1\x87",
    "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa3\xb8\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa1\x87\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa3\xb4\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\x83",
    "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa2\xa0\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\x87\xe2\xa0\x80\xe2\xa0\x80\xe2\xa2\x80\xe2\xa3\xbe\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\x80",
    "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa3\xb4\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xb7\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa1\x9f\xe2\xa0\x80",
    "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa2\xb0\xe2\xa1\xbf\xe2\xa0\x89\xe2\xa0\x80\xe2\xa1\x9c\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa1\xbf\xe2\xa0\xbf\xe2\xa2\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa0\x83\xe2\xa0\x80",
    "\xe2\xa0\x92\xe2\xa0\x92\xe2\xa0\xb8\xe2\xa3\xbf\xe2\xa3\x84\xe2\xa1\x98\xe2\xa3\x83\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa1\x9f\xe2\xa2\xb0\xe2\xa0\x83\xe2\xa0\x80\xe2\xa2\xb9\xe2\xa3\xbf\xe2\xa1\x87\xe2\xa0\x80",
    "\xe2\xa0\x9a\xe2\xa0\x89\xe2\xa0\x80\xe2\xa0\x88\xe2\xa0\xbb\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xbf\xe2\xa3\xae\xe2\xa3\xa4\xe2\xa3\xa4\xe2\xa3\xbf\xe2\xa1\x9f\xe2\xa0\x81\xe2\xa0\x80",
    "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x88\xe2\xa0\x99\xe2\xa0\x9b\xe2\xa0\x9b\xe2\xa0\x9b\xe2\xa0\x9b\xe2\xa0\x9b\xe2\xa0\x81\xe2\xa0\x80\xe2\xa0\x92\xe2\xa0\xa4",
    "\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x80\xe2\xa0\x91\xe2\xa0\x80\xe2\xa0\x80",
};

/* ── bongo art: ASCII fallback ── */
static const char *BONGO_ASCII[BC_H] = {
    "        .       .        ",
    "        |\\_---_/|        ",
    "       /   o_o   \\       ",
    "       |    U    |       ",
    "      \\  ._I_.  /       ",
    "        `-_____-'        ",
    "                         ",
    "     					   ",
    "                         ",
};

/* active bongo pointer — set at startup */
static const char **BONGO = NULL;


/* ------ data ------ */
typedef struct {
    bool   done, pinned;
    char   tag[MAX_TAG];
    char   text[MAX_LINE];
    char   desc[MAX_DESC];
    time_t created_at;
    time_t done_at;
    time_t sched_at;
} Todo;

/* a task is hidden while its schedule time is in the future */
static bool is_hidden(const Todo *t);

typedef struct {
    char   text[MAX_NOTE];
    time_t created_at;
} Note;

typedef enum { VIEW_TASKS, VIEW_NOTES, VIEW_SCHED } View;

static Todo todos[MAX_TODOS];
static int  todo_count = 0;
static int  sel        = 0;
static Note notes[MAX_NOTES];
static int  note_count = 0;
static int  sel_note   = 0;
static View view        = VIEW_TASKS;
static char search[MAX_LINE]    = "";
static char filter_tag[MAX_TAG] = "";
static int  vis[MAX_TODOS];
static int  vis_count  = 0;
static int  note_vis[MAX_NOTES];
static int  note_vis_count = 0;
static int  note_top   = 0;
static int  sched_vis[MAX_TODOS];
static int  sched_vis_count = 0;
static int  sel_sched = 0;
static int  sched_top = 0;
static char smsg[256]  = "";
static int  smsg_ttl   = 0;
static int  celebrate  = 0;

/* ------ tag cache ------ */
static char utags[MAX_UTAGS][MAX_TAG];
static int  utag_counts[MAX_UTAGS];
static int  tag_cnt_for[MAX_TODOS];
static int  utag_n     = 0;
static int  tags_dirty = 1;

/* frame-cached counts --- computed once per frame, not per draw call */
static int  g_pend=0, g_done_c=0, g_pin_c=0, g_sched_c=0;

/* ------ helpers ------ */
static int tag_color(const char *tag) {
    if (!tag || !tag[0] || strcmp(tag,"none")==0) return C_PEND;
    unsigned h = 0;
    for (const char *p=tag; *p; p++) h = h*31+(unsigned char)*p;
    static const int cols[]={C_TAG1,C_TAG2,C_TAG3,C_TAG4,C_TAG5,C_TAG6,C_TAG7,C_TAG8};
    return cols[h%8];
}

static const char *bongo_mood(int p) {
    if (p==0 && todo_count>0) return "  purrfect!! all done~ ";
    if (p==0)                  return "  add a task nya~";
    if (p==1)                  return "  one more! you got it!";
    if (p<=3)                  return "  almost there, meow~";
    if (p<=7)                  return "  *meow meow! you productive nya~*";
    return                            "  !!so many tasks hiss! ";
}

/* ── UTF-8 detection: check LC_ALL / LC_CTYPE / LANG for UTF-8 ── */
static bool detect_utf8(void) {
    const char *vars[] = { "LC_ALL", "LC_CTYPE", "LANG", NULL };
    for (int i = 0; vars[i]; i++) {
        const char *v = getenv(vars[i]);
        if (!v || !*v) continue;
        char buf[strlen(v) + 1]; int j;
        for (j=0; v[j] && j<63; j++) buf[j]=(char)tolower((unsigned char)v[j]);
        buf[j]='\0';
        if (strstr(buf,"utf")) return true;
    }
    return false;
}


static void set_smsg(const char *m) {
    strncpy(smsg,m,sizeof smsg-1); smsg[sizeof smsg-1]='\0'; smsg_ttl=5;
}

/* ------ paths ------ */
static char* todo_path;
static char* legacy_path;
size_t todo_path_length;

/* recursive mkdir (like mkdir -p) for the whole base path */
static void mkdir_p(const char *path) {
    char tmp[strlen(path) + 1];
    strcpy(tmp, path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static void build_paths(void) {
    #define LOCAL_BUFFER_SIZE 64

    const char *home=getenv("HOME");
    const char *xdg=getenv("XDG_DATA_HOME");

    size_t base_length = 2;
    if(home)
        base_length = strlen(home);
    if(xdg && strlen(xdg)>base_length)
        base_length = strlen(xdg);
    char base[base_length + 1 + LOCAL_BUFFER_SIZE];

    if(xdg && xdg[0])
        snprintf(base,sizeof base,"%s/meowdo",xdg);
    else if(home && home[0])
        snprintf(base,sizeof base,"%s/.local/share/meowdo",home);
    else
        strcpy(base, ".");

    mkdir_p(base);
    if (access(base, W_OK)!=0) {
        todo_path_length = LOCAL_BUFFER_SIZE;
        todo_path = malloc(todo_path_length);
        strcpy(todo_path, "meowdo.json");
        legacy_path = malloc(LOCAL_BUFFER_SIZE);
        strcpy(legacy_path, "todos.txt");
        return;
    }

    todo_path_length = strlen(base) + 1 + LOCAL_BUFFER_SIZE;
    todo_path = malloc(todo_path_length);
    snprintf(todo_path,todo_path_length,"%s/meowdo.json",base);
    legacy_path = malloc(todo_path_length);
    snprintf(legacy_path,todo_path_length,"%s/todos.txt",base);
}

/* ------ tag cache ------ */
static void rebuild_tag_cache(void) {
    utag_n=0;
    for (int i=0;i<todo_count;i++) {
        if (!todos[i].tag[0]||strcmp(todos[i].tag,"none")==0) continue;
        if (is_hidden(&todos[i])) continue;
        int found=-1;
        for (int j=0;j<utag_n;j++)
            if (strcmp(utags[j],todos[i].tag)==0){found=j;break;}
        if (found<0 && utag_n<MAX_UTAGS) {
            strncpy(utags[utag_n],todos[i].tag,MAX_TAG-1);
            utags[utag_n][MAX_TAG-1]='\0';
            utag_counts[utag_n]=1; utag_n++;
        } else if (found>=0) utag_counts[found]++;
    }
    for (int i=0;i<todo_count;i++) {
        tag_cnt_for[i]=0;
        for (int j=0;j<utag_n;j++)
            if (strcmp(utags[j],todos[i].tag)==0){tag_cnt_for[i]=utag_counts[j];break;}
    }
    tags_dirty=0;
}
static void mark_dirty(void){ tags_dirty=1; }

/* one pass to count pending/done/pinned/scheduled */
static void recount(void) {
    g_pend=0; g_done_c=0; g_pin_c=0; g_sched_c=0;
    for (int i=0;i<todo_count;i++){
        if(is_hidden(&todos[i])){ g_sched_c++; continue; }
        if(todos[i].done) g_done_c++; else g_pend++;
        if(todos[i].pinned) g_pin_c++;
    }
}

/* ------ JSON helpers (minimal, enough for our schema) ------ */
static void jw_string(FILE *f, const char *s) {
    fputc('"', f);
    for (const unsigned char *p=(const unsigned char*)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\b': fputs("\\b",  f); break;
        case '\f': fputs("\\f",  f); break;
        case '\n': fputs("\\n",  f); break;
        case '\r': fputs("\\r",  f); break;
        case '\t': fputs("\\t",  f); break;
        default:
            if (*p < 0x20) fprintf(f,"\\u%04x",*p);
            else fputc(*p, f);
        }
    }
    fputc('"', f);
}

typedef struct { const char *p; } JParser;

static void jp_ws(JParser *J) {
    while (*J->p==' '||*J->p=='\t'||*J->p=='\n'||*J->p=='\r') J->p++;
}
static int jp_peek(JParser *J) { jp_ws(J); return (unsigned char)*J->p; }
static int jp_ch(JParser *J, char c) {
    if (jp_peek(J)==(unsigned char)c) { J->p++; return 1; }
    return 0;
}

static int jp_hex4(JParser *J, unsigned *out) {
    unsigned v=0;
    for (int i=0;i<4;i++) {
        char c=J->p[i]; unsigned d;
        if      (c>='0'&&c<='9') d=(unsigned)(c-'0');
        else if (c>='a'&&c<='f') d=(unsigned)(c-'a')+10;
        else if (c>='A'&&c<='F') d=(unsigned)(c-'A')+10;
        else return 0;
        v=v*16+d;
    }
    J->p+=4; *out=v; return 1;
}

/* encode codepoint as UTF-8 into out; returns bytes written (1-4) */
static int utf8_put(unsigned cp, char *out) {
    if (cp<0x80)   { out[0]=(char)cp; return 1; }
    if (cp<0x800)  { out[0]=0xC0|(char)(cp>>6);  out[1]=0x80|(char)(cp&0x3F); return 2; }
    if (cp<0x10000){ out[0]=0xE0|(char)(cp>>12); out[1]=0x80|(char)((cp>>6)&0x3F);
                     out[2]=0x80|(char)(cp&0x3F); return 3; }
                     out[0]=0xF0|(char)(cp>>18);  out[1]=0x80|(char)((cp>>12)&0x3F);
                     out[2]=0x80|(char)((cp>>6)&0x3F); out[3]=0x80|(char)(cp&0x3F); return 4;
}

/* parse a quoted JSON string into out (UTF-8, NUL-terminated).
   over-long strings are truncated but still consumed. */
static int jp_string(JParser *J, char *out, size_t n) {
    if (!jp_ch(J,'"')) return 0;
    size_t w=0;
    while (*J->p && *J->p!='"') {
        char tmp[4]; int tl=0; size_t need;
        char c=*J->p;
        if (c=='\\') {
            J->p++;
            char e=*J->p++;
            switch (e) {
            case '"':  tmp[0]='"';  tl=1; break;
            case '\\': tmp[0]='\\'; tl=1; break;
            case '/':  tmp[0]='/';  tl=1; break;
            case 'b':  tmp[0]='\b'; tl=1; break;
            case 'f':  tmp[0]='\f'; tl=1; break;
            case 'n':  tmp[0]='\n'; tl=1; break;
            case 'r':  tmp[0]='\r'; tl=1; break;
            case 't':  tmp[0]='\t'; tl=1; break;
            case 'u': {
                unsigned cp;
                if (!jp_hex4(J,&cp)) return 0;
                if (cp>=0xD800&&cp<=0xDBFF) {  /* high surrogate */
                    const char *sv=J->p;
                    J->p+=2;                    /* skip "\u" of low surrogate */
                    unsigned lo;
                    if (jp_hex4(J,&lo)&&lo>=0xDC00&&lo<=0xDFFF)
                        cp=0x10000+((cp-0xD800)<<10)+(lo-0xDC00);
                    else J->p=sv;               /* not a pair: leave as-is */
                }
                tl=utf8_put(cp,tmp);
                break;
            }
            default: return 0;
            }
        } else {
            tmp[0]=c; tl=1; J->p++;
        }
        need=(size_t)tl;
        if (w+need<n) { memcpy(out+w,tmp,need); w+=need; }
        else if (w<n) { memcpy(out+w,tmp,n-1-w); w=n; }
    }
    if (*J->p!='"') return 0;
    J->p++;
    if (n>0) { if (w>=n) w=n-1; out[w]='\0'; }
    return 1;
}

static bool jp_bool(JParser *J) {
    jp_ws(J);
    if (!strncmp(J->p,"true",4))  { J->p+=4; return true; }
    if (!strncmp(J->p,"false",5)) { J->p+=5; return false; }
    return false;
}

static long long jp_number(JParser *J) {
    jp_ws(J);
    char *end;
    long long v=strtoll(J->p,&end,10);
    if (end==J->p) return 0;
    J->p=end;
    /* tolerate a fractional/exponent part (only integers are stored) */
    if (*J->p=='.') {
        J->p++;
        while (*J->p>='0'&&*J->p<='9') J->p++;
        if (*J->p=='e'||*J->p=='E'){
            J->p++;
            if (*J->p=='+'||*J->p=='-') J->p++;
            while (*J->p>='0'&&*J->p<='9') J->p++;
        }
    }
    return v;
}

/* skip any JSON value (used for unknown fields) */
static void jp_skip_value(JParser *J) {
    int c=jp_peek(J);
    if (c=='"') { J->p++;
        while (*J->p && *J->p!='"') {
            if (*J->p=='\\'&&J->p[1]) J->p++;
            J->p++;
        }
        if (*J->p) J->p++;
        return;
    }
    if (c=='[') {
        J->p++;
        for(;;) {
            int ch=jp_peek(J);
            if (!ch) return;
            if (ch==']') { J->p++; return; }
            if (ch==',') { J->p++; continue; }
            jp_skip_value(J);
        }
    }
    if (c=='{') {
        J->p++;
        for(;;) {
            int ch=jp_peek(J);
            if (!ch) return;
            if (ch=='}') { J->p++; return; }
            if (ch==',') { J->p++; continue; }
            if (ch==':') { J->p++; continue; }
            jp_skip_value(J);
        }
    }
    if (!strncmp(J->p,"true",4))  { J->p+=4; return; }
    if (!strncmp(J->p,"false",5)) { J->p+=5; return; }
    if (!strncmp(J->p,"null",4))  { J->p+=4; return; }
    while (*J->p && strchr("+-.0123456789eE",*J->p)) J->p++;
}

/* ------ I/O ------ */
static int jp_task_obj(JParser *J) {
    Todo t; memset(&t,0,sizeof t);
    t.done_at=0;
    for(;;) {
        char key[32];
        int ch=jp_peek(J);
        if (ch=='}') { J->p++; break; }
        if (ch==',') { J->p++; continue; }
        if (ch!='"') return 0;
        if (!jp_string(J,key,sizeof key)) return 0;
        if (!jp_ch(J,':')) return 0;
        if      (!strcmp(key,"pinned"))  t.pinned=jp_bool(J);
        else if (!strcmp(key,"done"))    t.done=jp_bool(J);
        else if (!strcmp(key,"tag"))     jp_string(J,t.tag,MAX_TAG);
        else if (!strcmp(key,"text"))    jp_string(J,t.text,MAX_LINE);
        else if (!strcmp(key,"desc"))    jp_string(J,t.desc,MAX_DESC);
        else if (!strcmp(key,"created")) t.created_at=(time_t)jp_number(J);
        else if (!strcmp(key,"done_at")) t.done_at=(time_t)jp_number(J);
        else if (!strcmp(key,"sched_at")) t.sched_at=(time_t)jp_number(J);
        else jp_skip_value(J);
    }
    if (todo_count<MAX_TODOS) todos[todo_count++]=t;
    return 1;
}

static int jp_note_obj(JParser *J) {
    Note n; memset(&n,0,sizeof n);
    for(;;) {
        char key[32];
        int ch=jp_peek(J);
        if (ch=='}') { J->p++; break; }
        if (ch==',') { J->p++; continue; }
        if (ch!='"') return 0;
        if (!jp_string(J,key,sizeof key)) return 0;
        if (!jp_ch(J,':')) return 0;
        if      (!strcmp(key,"text"))    jp_string(J,n.text,MAX_NOTE);
        else if (!strcmp(key,"created")) n.created_at=(time_t)jp_number(J);
        else jp_skip_value(J);
    }
    if (note_count<MAX_NOTES) notes[note_count++]=n;
    return 1;
}

static int todos_load(void) {
    FILE *f=fopen(todo_path,"rb"); if(!f) return 0;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if (sz<0||sz>1<<20) { fclose(f); return 0; }
    char *buf=malloc((size_t)sz+1);
    if (!buf) { fclose(f); return 0; }
    size_t n=fread(buf,1,(size_t)sz,f);
    buf[n]='\0';
    fclose(f);

    JParser J={buf};
    int ok=0;
    todo_count=0; note_count=0;
    if (jp_ch(&J,'{')) {
        ok=1;
        for(;;) {
            char key[32];
            int ch=jp_peek(&J);
            if (ch=='}') { J.p++; break; }
            if (ch==',') { J.p++; continue; }
            if (ch!='"') { ok=0; break; }
            if (!jp_string(&J,key,sizeof key)) { ok=0; break; }
            if (!jp_ch(&J,':')) { ok=0; break; }
            if (!strcmp(key,"tasks")) {
                if (!jp_ch(&J,'[')) { ok=0; break; }
                for(;;) {
                    int c2=jp_peek(&J);
                    if (c2==']') { J.p++; break; }
                    if (c2==',') { J.p++; continue; }
                    if (c2!='{') { ok=0; break; }
                    J.p++;
                    if (!jp_task_obj(&J)) { ok=0; break; }
                }
            }
            else if (!strcmp(key,"notes")) {
                if (!jp_ch(&J,'[')) { ok=0; break; }
                for(;;) {
                    int c2=jp_peek(&J);
                    if (c2==']') { J.p++; break; }
                    if (c2==',') { J.p++; continue; }
                    if (c2!='{') { ok=0; break; }
                    J.p++;
                    if (!jp_note_obj(&J)) { ok=0; break; }
                }
            }
            else jp_skip_value(&J);
        }
    }
    free(buf);
    return ok;
}

static void todos_save(void) {
    FILE *f=fopen(todo_path,"w"); if(!f) return;
    fprintf(f,"{\n \"version\": 1,\n \"tasks\": [\n");
    for (int i=0;i<todo_count;i++){
        Todo *t=&todos[i];
        fprintf(f,"  {\"pinned\": %s, \"done\": %s, \"tag\": ",
            t->pinned?"true":"false", t->done?"true":"false");
        jw_string(f,t->tag[0]?t->tag:"none");
        fprintf(f,", \"text\": "); jw_string(f,t->text);
        fprintf(f,", \"desc\": ");  jw_string(f,t->desc);
        fprintf(f,", \"created\": %ld, \"done_at\": %ld, \"sched_at\": %ld }%s\n",
            (long)t->created_at,(long)t->done_at,(long)t->sched_at, i+1<todo_count?",":"");
    }
    fprintf(f," ],\n \"notes\": [\n");
    for (int i=0;i<note_count;i++){
        Note *nt=&notes[i];
        fprintf(f,"  {\"text\": ");
        jw_string(f,nt->text);
        fprintf(f,", \"created\": %ld }%s\n",
            (long)nt->created_at, i+1<note_count?",":"");
    }
    fprintf(f," ]\n}\n");
    fclose(f);
}

/* one-time import of the old pipe-delimited todos.txt */
static int todos_import_legacy(void) {
    FILE *f=fopen(legacy_path,"r"); if(!f) return 0;
    todo_count=0;
    char line[MAX_LINE+MAX_TAG+64];
    while (todo_count<MAX_TODOS && fgets(line,(int)sizeof line,f)) {
        int len=(int)strlen(line);
        if (len>0&&line[len-1]=='\n') line[--len]='\0';
        if (len<5) continue;
        Todo *t=&todos[todo_count];
        t->done_at=0;
        t->tag[0]='\0';t->text[0]='\0';t->desc[0]='\0';
        t->sched_at=0;
        t->pinned=(line[0]=='P');
        t->done  =(line[2]=='x');
        char *p1=strchr(line+4,'|'); if(!p1) continue;
        int tlen=(int)(p1-(line+4));
        if(tlen<0) continue;
        if(tlen>=MAX_TAG) tlen=MAX_TAG-1;
        snprintf(t->tag, MAX_TAG, "%.*s", tlen, line+4);
        char *p2=p1+1, *p3=strchr(p2,'|');
        if(p3){
            int txtlen=(int)(p3-p2); if(txtlen<0)txtlen=0;
            if(txtlen>=MAX_LINE)txtlen=MAX_LINE-1;
            strncpy(t->text,p2,(size_t)txtlen); t->text[txtlen]='\0';
            char *p4=strchr(p3+1,'|');
            t->created_at=(time_t)atol(p3+1);
            if(p4) t->done_at=(time_t)atol(p4+1);
        } else {
            strncpy(t->text,p2,MAX_LINE-1); t->text[MAX_LINE-1]='\0';
            t->created_at=time(NULL);
        }
        todo_count++;
    }
    fclose(f);
    return todo_count>0;
}

/* ------ sort / filter ------ */
/* sort class: 0=pinned+pending  1=pinned+done  2=pending  3=done */
static int todo_class(const Todo *t) {
    return (t->pinned?0:2) + (t->done?1:0);
}

static void rebuild_vis(void) {
    /* stable class sort — preserves manual (J/K) order within groups */
    static Todo sorted[MAX_TODOS];
    int n=0;
    for(int c=0;c<4;c++)
        for(int i=0;i<todo_count;i++)
            if(todo_class(&todos[i])==c) sorted[n++]=todos[i];
    memcpy(todos,sorted,(size_t)todo_count*sizeof(Todo));
    vis_count=0;
    char ndl[MAX_LINE]="";
    if (search[0]) {
        snprintf(ndl,sizeof ndl,"%s",search);
        for(int j=0;ndl[j];j++) ndl[j]=(char)tolower((unsigned char)ndl[j]);
    }
    for(int i=0;i<todo_count;i++){
        Todo *t=&todos[i];
        if(is_hidden(t)) continue;
        if(filter_tag[0]&&strcmp(t->tag,filter_tag)!=0) continue;
        if(search[0]){
            char hay[MAX_LINE];
            snprintf(hay,sizeof hay,"%s",t->text);
            for(int j=0;hay[j];j++) hay[j]=(char)tolower((unsigned char)hay[j]);
            if(!strstr(hay,ndl)) continue;
        }
        vis[vis_count++]=i;
    }
    if(sel>=vis_count) sel=vis_count>0?vis_count-1:0;
    mark_dirty();
}

static void rebuild_note_vis(void) {
    note_vis_count=0;
    char ndl[MAX_LINE]="";
    if (search[0]) {
        snprintf(ndl,sizeof ndl,"%s",search);
        for(int j=0;ndl[j];j++) ndl[j]=(char)tolower((unsigned char)ndl[j]);
    }
    for(int i=0;i<note_count;i++){
        Note *n=&notes[i];
        if(search[0]){
            char hay[MAX_NOTE];
            strncpy(hay,n->text,sizeof hay-1);
            hay[sizeof hay-1]='\0';
            for(int j=0;hay[j];j++) hay[j]=(char)tolower((unsigned char)hay[j]);
            if(!strstr(hay,ndl)) continue;
        }
        note_vis[note_vis_count++]=i;
    }
    if(sel_note>=note_vis_count) sel_note=note_vis_count>0?note_vis_count-1:0;
}

static void rebuild_sched_vis(void) {
    sched_vis_count=0;
    char ndl[MAX_LINE]="";
    if (search[0]) {
        snprintf(ndl,sizeof ndl,"%s",search);
        for(int j=0;ndl[j];j++) ndl[j]=(char)tolower((unsigned char)ndl[j]);
    }
    for(int i=0;i<todo_count;i++){
        Todo *t=&todos[i];
        if(!is_hidden(t)) continue;
        if(search[0]){
            char hay[MAX_LINE];
            snprintf(hay,sizeof hay,"%s",t->text);
            for(int j=0;hay[j];j++) hay[j]=(char)tolower((unsigned char)hay[j]);
            if(!strstr(hay,ndl)) continue;
        }
        sched_vis[sched_vis_count++]=i;
    }
    /* soonest first: insertion sort by sched_at */
    for(int a=1;a<sched_vis_count;a++){
        int v=sched_vis[a], b=a-1;
        while(b>=0 && todos[sched_vis[b]].sched_at>todos[v].sched_at){
            sched_vis[b+1]=sched_vis[b]; b--;
        }
        sched_vis[b+1]=v;
    }
    if(sel_sched>=sched_vis_count) sel_sched=sched_vis_count>0?sched_vis_count-1:0;
}

/* move task to array position 0 (displayed at top of its sort group) */
static void move_to_front(int i) {
    Todo t=todos[i];
    for (int k=i;k>0;k--) todos[k]=todos[k-1];
    todos[0]=t;
}

/* reveal every task whose schedule time has arrived: move to the top
   of the list (below pinned, which the class sort keeps above) and
   clear sched_at. returns true if anything was revealed. */
static bool process_reveals(void) {
    time_t now=time(NULL);
    int idx[MAX_TODOS], nidx=0;
    for (int i=0;i<todo_count;i++)
        if (todos[i].sched_at && todos[i].sched_at<=now) idx[nidx++]=i;
    if (!nidx) return false;

    /* sort revealed indices newest-first so that, when each is moved to
       the front, the oldest ends up on top */
    for (int a=0;a<nidx;a++)
        for (int b=a+1;b<nidx;b++)
            if (todos[idx[b]].sched_at>todos[idx[a]].sched_at){
                int t=idx[a]; idx[a]=idx[b]; idx[b]=t;
            }

    static Todo scratch[MAX_TODOS];
    static unsigned char taken[MAX_TODOS];
    memset(taken,0,(size_t)todo_count);
    for (int a=0;a<nidx;a++) taken[idx[a]]=1;

    int n=0;
    for (int a=nidx-1;a>=0;a--){          /* oldest first at the top */
        scratch[n]=todos[idx[a]];
        scratch[n].sched_at=0;
        n++;
    }
    for (int i=0;i<todo_count;i++)
        if (!taken[i]) scratch[n++]=todos[i];
    memcpy(todos,scratch,(size_t)n*sizeof(Todo));
    return true;
}

/* ------ timestamp ------ */
static void fmt_ts(time_t ts,char *buf,int buflen){
    if(!ts){strncpy(buf,"-",(size_t)buflen);buf[buflen-1]='\0';return;}
    struct tm tmbuf; struct tm *tm=localtime_r(&ts,&tmbuf);
    if(tm) strftime(buf,(size_t)buflen,"%d %b %H:%M",tm);
    else   strncpy(buf,"?",(size_t)buflen);
}

/* short countdown until a future time, like "2d 3h" / "3h 20m" / "45m" */
static void fmt_in(time_t when,char *buf,int buflen){
    long long d=(long long)when-(long long)time(NULL);
    if(d<0) d=0;
    long long days=d/86400; d%=86400;
    long long h=d/3600; d%=3600;
    long long m=d/60;
    if(days>0)      snprintf(buf,(size_t)buflen,"%lldd %lldh",days,h);
    else if(h>0)    snprintf(buf,(size_t)buflen,"%lldh %lldm",h,m);
    else            snprintf(buf,(size_t)buflen,"%lldm",m);
}

/* parseable date+time, "2026-11-25 09:00" (used for schedule popups) */
static void fmt_ymd(time_t ts,char *buf,int buflen){
    struct tm tmbuf; struct tm *tm=localtime_r(&ts,&tmbuf);
    if(tm) strftime(buf,(size_t)buflen,"%Y-%m-%d %H:%M",tm);
    else   strncpy(buf,"?",(size_t)buflen);
}

/* schedule popup hint showing the current date+time */
static void sched_hint(char *buf,int buflen){
    char nowbuf[32];
    fmt_ymd(time(NULL),nowbuf,(int)sizeof nowbuf);
    snprintf(buf,(size_t)buflen,"now %s | e.g. +2h | 09:00 | tomorrow 09:00",nowbuf);
}

/* a task is hidden while its schedule time is in the future */
static bool is_hidden(const Todo *t){
    return t->sched_at!=0 && t->sched_at>time(NULL);
}

/* ------ schedule time parsing ------
   accepts:  +30m  +2h  +3d  +1w   (repeatable: "+1d 2h")
             HH:MM                 (today, or tomorrow if already past)
             [today|tomorrow] HH:MM
             YYYY-MM-DD [HH:MM]
   returns false (leaving *out untouched) on anything else.        */
static bool parse_time_input(const char *s, time_t *out){
    while (*s==' '||*s=='\t') s++;
    if (!*s) return false;

    time_t now=time(NULL);

    /* relative offsets: +N(m|h|d|w), tokens combinable: "+1d 2h" */
    if (*s=='+') {
        long long total=0;
        const char *p=s+1;
        for(;;){
            char *end;
            long long n=strtoll(p,&end,10);
            if (end==p||n<=0) return false;
            p=end;
            long long mult;
            switch (tolower((unsigned char)*p)) {
            case 'm': mult=60;     break;
            case 'h': mult=3600;   break;
            case 'd': mult=86400;  break;
            case 'w': mult=604800; break;
            default:  return false;
            }
            p++;
            total+=n*mult;
            while (*p==' '||*p=='\t') p++;
            if (!*p){ *out=now+total; return true; }
        }
    }

    struct tm base;
    if (!localtime_r(&now,&base)) return false;
    bool have_date=false;
    int y=0,mo=0,dd=0,hh=-1,mm=-1;
    const char *p=s;

    if      (!strncasecmp(p,"today",5))    { p+=5; have_date=true; }
    else if (!strncasecmp(p,"tomorrow",8)) { p+=8; base.tm_mday++; have_date=true; }
    else {
        int consumed=0;
        if (sscanf(p,"%4d-%2d-%2d%n",&y,&mo,&dd,&consumed)==3 && consumed==10) {
            base.tm_year=y-1900; base.tm_mon=mo-1; base.tm_mday=dd;
            have_date=true; p+=consumed;
        }
    }
    while (*p==' '||*p=='\t') p++;

    if (*p) {                      /* HH:MM part */
        int consumed=0;
        if (sscanf(p,"%2d:%2d%n",&hh,&mm,&consumed)!=2) return false;
        p+=consumed;
        while (*p==' '||*p=='\t') p++;
    }
    if (*p) return false;          /* trailing junk */

    if (hh<0){ hh=0; mm=0; }
    if (mm<0) mm=0;
    if (mo>12||dd>31||hh>23||mm>59) return false;
    if ((mo||dd)&&(mo<1||dd<1)) return false;

    struct tm target=base;
    target.tm_hour=hh; target.tm_min=mm; target.tm_sec=0; target.tm_isdst=-1;
    time_t when=mktime(&target);
    if (when==(time_t)-1) return false;

    /* reject non-existent dates (e.g. Feb 30) that mktime silently normalises */
    if (have_date) {
        struct tm chk;
        if (!localtime_r(&when,&chk)) return false;
        if (chk.tm_year!=base.tm_year || chk.tm_mon!=base.tm_mon || chk.tm_mday!=base.tm_mday)
            return false;
    }

    if (!have_date && when<=now) {   /* bare HH:MM in the past -> tomorrow */
        when+=86400;
        if (!localtime_r(&when,&target)) return false;
        target.tm_hour=hh; target.tm_min=mm; target.tm_sec=0; target.tm_isdst=-1;
        when=mktime(&target);
        if (when==(time_t)-1) return false;
    }
    *out=when;
    return true;
}

/* ------ layout detection ------ */
static Layout get_layout(int rows, int cols) {
    if (rows < MIN_ROWS || cols < MIN_COLS_COMPACT) return LAYOUT_TOOSMALL;
    if (cols < MIN_COLS_FULL)                        return LAYOUT_COMPACT;
    return LAYOUT_FULL;
}


/* ------ utf-8 multibyte helpers ------ */
/* safely truncate text to fit display width; returns display cols used */
static int trunc_to_width(const char *src, char *dst, int dst_size, int max_width){
    int w_used=0, byte_len=0;
    while(*src && byte_len<dst_size-1){
        wchar_t wc;
        int mb=mbtowc(&wc,src,MB_CUR_MAX);
        if(mb<=0) break;
        int wcw=wcwidth(wc); if(wcw<0) wcw=0;
        if(w_used+wcw>max_width) break;
        memcpy(dst,src,(size_t)mb);
        dst+=mb; src+=mb; w_used+=wcw; byte_len+=mb;
    }
    *dst='\0';
    return w_used;
}

static void popup_draw_field(WINDOW *win, int start_row, int max_row, int field_w,
                             const char *s, int cur_byte,
                             int *cur_row_out, int *cur_col_out)
{
    /* clear only the valid field rows — never touch hint bar or border */
    for (int r = start_row; r <= max_row; r++)
        mvwprintw(win, r, 3, "%-*s", field_w + 2, "");

    wattron(win, COLOR_PAIR(C_PEND)|A_BOLD);
    mvwprintw(win, start_row, 3, "> ");
    wattroff(win, COLOR_PAIR(C_PEND)|A_BOLD);

    int row = start_row;
    int col = 5;          /* column after "> " */
    int col_used = 0;     /* display columns used on this line */
    const char *p = s;
    int off = 0;          /* byte offset of next char */
    bool cursor_set = (cur_byte == 0);
    int crow = row, ccol = col;

    while (*p) {
        wchar_t wc;
        int mb = mbtowc(&wc, p, MB_CUR_MAX);
        if (mb <= 0) { p++; off++; continue; }   /* skip invalid bytes */

        int wcw = wcwidth(wc);
        if (wcw < 0) wcw = 0;             /* non-printable -> 0 width */

        /* wrap to next line if this char doesn't fit */
        if (col_used + wcw > field_w) {
            /* pad remainder of current line */
            wattron(win, COLOR_PAIR(C_PEND)|A_BOLD);
            mvwprintw(win, row, col, "%-*s", field_w - col_used, "");
            wattroff(win, COLOR_PAIR(C_PEND)|A_BOLD);
            if (row >= max_row) break;     /* no room — stop drawing */
            row++;
            col = 5; col_used = 0;
            /* continuation lines: two-space indent to align with "> " */
            wattron(win, COLOR_PAIR(C_PEND)|A_BOLD);
            mvwprintw(win, row, 3, "  ");
            wattroff(win, COLOR_PAIR(C_PEND)|A_BOLD);
        }

        /* record the screen position of the text cursor */
        if (!cursor_set && off == cur_byte) { crow = row; ccol = col; cursor_set = true; }

        /* print the character */
        char mb_buf[MB_CUR_MAX + 1];
        memcpy(mb_buf, p, (size_t)mb);
        mb_buf[mb] = '\0';
        wattron(win, COLOR_PAIR(C_PEND)|A_BOLD);
        mvwprintw(win, row, col, "%s", mb_buf);
        wattroff(win, COLOR_PAIR(C_PEND)|A_BOLD);

        col      += wcw;
        col_used += wcw;
        p        += mb;
        off      += mb;
    }

    /* cursor at end of text */
    if (!cursor_set) { crow = row; ccol = col; }
    *cur_row_out = crow;
    *cur_col_out = ccol;
}

static int popup_needed_rows(const char *s, int field_w) {
    if (!s || !s[0] || field_w < 1) return 1;
    int rows = 1, col_used = 0;
    const char *p = s;
    while (*p) {
        wchar_t wc;
        int mb = mbtowc(&wc, p, MB_CUR_MAX);
        if (mb <= 0) { p++; continue; }
        int wcw = wcwidth(wc); if (wcw < 0) wcw = 0;
        if (col_used + wcw > field_w) { rows++; col_used = 0; }
        col_used += wcw;
        p += mb;
    }
    return rows;
}

/* ------ draw: word-wrap utf-8 text (with \n paragraphs) ------
   draws into rows [row..max_row], columns [x..x+width).
   every emitted line is padded to the full width so a shorter
   redraw never leaves stale cells behind.
   *trunc is set if the source didn't fit. returns next free row. */
static int draw_wrapped(WINDOW *w, int row, int max_row, int width, int x,
                        const char *src, bool *trunc) {
    int col = 0;
    if (trunc) *trunc = false;
    #define DW_PAD() do { \
        if (col < width && row <= max_row) \
            mvwprintw(w, row, x + col, "%*s", width - col, ""); \
    } while (0)
    while (*src) {
        if (*src == '\n') {
            DW_PAD();
            row++; col = 0; src++;
            continue;
        }
        if (row > max_row) { if (trunc) *trunc = true; break; }
        wchar_t wc;
        int mb = mbtowc(&wc, src, MB_CUR_MAX);
        if (mb <= 0) { src++; continue; }
        int wcw = wcwidth(wc); if (wcw < 0) wcw = 0;
        if (col + wcw > width) {
            DW_PAD();
            row++; col = 0;
            continue;
        }
        char mb_buf[MB_CUR_MAX + 1];
        memcpy(mb_buf, src, (size_t)mb);
        mb_buf[mb] = '\0';
        mvwprintw(w, row, x + col, "%s", mb_buf);
        col += wcw;
        src += mb;
    }
    DW_PAD();
    return row;
    #undef DW_PAD
}

/* ------ multiline editor (ported from pipboy's editor.cpp) ------ */
typedef struct {
    wchar_t lines[ML_LINES][ML_COLS];
    int nlines;
    int cur;
    int col;
    bool accepted;
} MLEditor;

static int ml_linelen(const MLEditor *e, int i) {
    int len = 0;
    while (len < ML_COLS && e->lines[i][len]) len++;
    return len;
}

static void ml_init(MLEditor *e, const char *initial) {
    memset(e, 0, sizeof *e);
    int line = 0, colc = 0;
    const char *p = initial;
    while (*p && line < ML_LINES) {
        if (*p == '\n') {
            if (line + 1 < ML_LINES) { line++; colc = 0; }
            p++;
            continue;
        }
        wchar_t wc;
        int mb = mbtowc(&wc, p, MB_CUR_MAX);
        if (mb <= 0) { p++; continue; }
        if (colc < ML_COLS) e->lines[line][colc++] = wc;
        p += mb;
    }
    e->nlines = line + 1;
    e->cur = line;
    e->col = colc;
    if (e->col > ml_linelen(e, e->cur)) e->col = ml_linelen(e, e->cur);
}

static void ml_serialize(const MLEditor *e, char *out, size_t n) {
    size_t w = 0;
    for (int i = 0; i < e->nlines && w + 1 < n; i++) {
        if (i) out[w++] = '\n';
        for (int c = 0; c < ML_COLS && e->lines[i][c] && w + 5 < n; c++) {
            char tmp[4];
            int tl = utf8_put((unsigned)e->lines[i][c], tmp);
            memcpy(out + w, tmp, (size_t)tl);
            w += (size_t)tl;
        }
    }
    if (w < n) out[w] = '\0'; else out[n-1] = '\0';
}

static void ml_insert(MLEditor *e, wchar_t wc) {
    int len = ml_linelen(e, e->cur);
    if (len >= ML_COLS) return;
    if (e->col > len) e->col = len;
    for (int c = len; c > e->col; c--)
        e->lines[e->cur][c] = e->lines[e->cur][c - 1];
    e->lines[e->cur][e->col++] = wc;
}

static void ml_backspace(MLEditor *e) {
    if (e->col > 0) {
        for (int c = e->col - 1; c < ML_COLS - 1; c++)
            e->lines[e->cur][c] = e->lines[e->cur][c + 1];
        e->lines[e->cur][ML_COLS - 1] = 0;
        e->col--;
    } else if (e->cur > 0) {
        int plen = ml_linelen(e, e->cur - 1);
        int len  = ml_linelen(e, e->cur);
        for (int c = 0; c < len && plen + c < ML_COLS - 1; c++)
            e->lines[e->cur - 1][plen + c] = e->lines[e->cur][c];
        for (int i = e->cur; i < e->nlines - 1; i++)
            memcpy(e->lines[i], e->lines[i + 1], sizeof e->lines[0]);
        memset(e->lines[e->nlines - 1], 0, sizeof e->lines[0]);
        e->nlines--;
        e->cur--;
        e->col = plen;
    }
}

static void ml_delete(MLEditor *e) {
    int len = ml_linelen(e, e->cur);
    if (e->col < len) {
        for (int c = e->col; c < ML_COLS - 1; c++)
            e->lines[e->cur][c] = e->lines[e->cur][c + 1];
        e->lines[e->cur][ML_COLS - 1] = 0;
    } else if (e->cur + 1 < e->nlines) {
        int nlen = ml_linelen(e, e->cur + 1);
        for (int c = 0; c < nlen && len + c < ML_COLS - 1; c++)
            e->lines[e->cur][len + c] = e->lines[e->cur + 1][c];
        for (int i = e->cur + 1; i < e->nlines - 1; i++)
            memcpy(e->lines[i], e->lines[i + 1], sizeof e->lines[0]);
        memset(e->lines[e->nlines - 1], 0, sizeof e->lines[0]);
        e->nlines--;
    }
}

static void ml_enter(MLEditor *e) {
    if (e->nlines >= ML_LINES) return;
    int len = ml_linelen(e, e->cur);
    int tn = len - e->col;
    if (tn < 0) tn = 0;
    wchar_t tail[ML_COLS] = {0};
    for (int c = 0; c < tn; c++) tail[c] = e->lines[e->cur][e->col + c];
    for (int c = e->col; c < ML_COLS; c++) e->lines[e->cur][c] = 0;
    for (int i = e->nlines; i > e->cur + 1; i--)
        memcpy(e->lines[i], e->lines[i - 1], sizeof e->lines[0]);
    memcpy(e->lines[e->cur + 1], tail, sizeof tail);
    e->nlines++;
    e->cur++;
    e->col = 0;
}

static void ml_move(MLEditor *e, int dline, int dcol, bool to_edge) {
    if (dline) {
        int n = e->cur + dline;
        if (n < 0) n = 0;
        if (n >= e->nlines) n = e->nlines - 1;
        e->cur = n;
        int len = ml_linelen(e, e->cur);
        if (to_edge)           e->col = dline > 0 ? 0 : len;
        else if (e->col > len) e->col = len;
    } else {
        int len = ml_linelen(e, e->cur);
        if (to_edge) e->col = dcol > 0 ? len : 0;
        else {
            e->col += dcol;
            if (e->col < 0)   e->col = 0;
            if (e->col > len) e->col = len;
        }
    }
}

static void ml_draw(MLEditor *e, WINDOW *win, int H, int W,
                    const char *title, int scroll) {
    (void)W;
    werase(win);
    wattron(win,COLOR_PAIR(C_BORDER)|A_BOLD); box(win,0,0); wattroff(win,COLOR_PAIR(C_BORDER)|A_BOLD);
    wattron(win,COLOR_PAIR(C_TOPBAR)|A_BOLD); mvwprintw(win,0,2," %s ",title); wattroff(win,COLOR_PAIR(C_TOPBAR)|A_BOLD);
    wattron(win,A_DIM);
    mvwprintw(win,H-1,2," Enter:new line  Ctrl-G:save  Esc:cancel ");
    wattroff(win,A_DIM);

    int textTop = 2;
    int maxVisible = H - 5;
    for (int vi = scroll; vi < e->nlines && vi - scroll < maxVisible; vi++) {
        int len = ml_linelen(e, vi);
        wmove(win, textTop + (vi - scroll), 2);
        if (vi == e->cur) {
            wchar_t cur_ch = e->col < len ? e->lines[vi][e->col] : L' ';
            if (e->col > 0) waddnwstr(win, e->lines[vi], e->col);
            cchar_t cc;
            setcchar(&cc, &cur_ch, A_REVERSE, 0, NULL);
            wadd_wch(win, &cc);
            if (len > e->col + 1)
                waddnwstr(win, e->lines[vi] + e->col + 1, len - e->col - 1);
        } else if (len > 0) {
            waddwstr(win, e->lines[vi]);
        }
    }
    /* scroll indicator */
    if (e->nlines > maxVisible) {
        wattron(win,A_DIM);
        if (scroll > 0) mvwaddch(win, textTop, 0, (chtype)'^');
        if (scroll + maxVisible < e->nlines) mvwaddch(win, H-2, 0, (chtype)'v');
        wattroff(win,A_DIM);
    }
}

static int popup_edit_ml(const char *title, char *out, size_t outlen) {
    int rows, cols; getmaxyx(stdscr, rows, cols);
    int pw = (cols < 74) ? cols - 4 : 74;
    int ph = (rows < 16) ? rows - 2 : 14;
    if (pw < 30 || ph < 8) return 0;
    int px = (cols - pw) / 2;
    int py = (rows - ph) / 2;
    WINDOW *p = newwin(ph, pw, py, px);
    if (!p) return 0;
    keypad(p, TRUE);

    MLEditor ed;
    ml_init(&ed, out);
    int scroll = 0;

    bool cancelled = false;
    for (;;) {
        int maxVisible = ph - 5;
        if (ed.cur < scroll) scroll = ed.cur;
        if (ed.cur >= scroll + maxVisible) scroll = ed.cur - maxVisible + 1;
        if (scroll < 0) scroll = 0;

        ml_draw(&ed, p, ph, pw, title, scroll);
        wmove(p, 2 + ed.cur - scroll, 2 + ed.col);
        wrefresh(p);

        wint_t ch;
        int rc = get_wch(&ch);
        if (rc == ERR) continue;
        if (rc == KEY_CODE_YES) {
            switch (ch) {
            case KEY_UP:    ml_move(&ed,-1,0,false); break;
            case KEY_DOWN:  ml_move(&ed, 1,0,false); break;
            case KEY_LEFT:  ml_move(&ed,0,-1,false); break;
            case KEY_RIGHT: ml_move(&ed,0, 1,false); break;
            case KEY_HOME:  ml_move(&ed,0,-1,true);  break;
            case KEY_END:   ml_move(&ed,0, 1,true);  break;
            case KEY_PPAGE: ml_move(&ed,-(ML_LINES/2),0,false); break;
            case KEY_NPAGE: ml_move(&ed, (ML_LINES/2),0,false); break;
            case KEY_BACKSPACE: ml_backspace(&ed); break;
            case KEY_DC:    ml_delete(&ed); break;
            case KEY_F(2):  ed.accepted = true; break;
            case KEY_MOUSE: { MEVENT ev; while (getmouse(&ev)==OK) {} } break;
            case KEY_RESIZE: default: break;
            }
        } else {
            switch (ch) {
            case 27: cancelled = true; break;
            case '\n': case '\r': ml_enter(&ed); break;
            case 0x07: case 0x13: ed.accepted = true; break; /* Ctrl-G / Ctrl-S */
            case '\b': case 127: ml_backspace(&ed); break;
            default:
                if (ch >= 32) ml_insert(&ed, (wchar_t)ch);
                break;
            }
        }
        if (ed.accepted || cancelled) break;
    }

    if (ed.accepted) ml_serialize(&ed, out, outlen);
    delwin(p);
    touchwin(stdscr); refresh();
    return ed.accepted && !cancelled;
}

/* ------ read-only multiline viewer (ported from pipboy) ------ */
static void view_multiline(const char *title, const char *text) {
    static wchar_t lines[256][ML_COLS + 8];
    int nlines = 0, colc = 0;
    const char *p = text;
    bool truncated = false;
    while (*p) {
        if (*p == '\n') {
            if (nlines + 1 < 256) { nlines++; colc = 0; p++; continue; }
            truncated = true; break;
        }
        wchar_t wc;
        int mb = mbtowc(&wc, p, MB_CUR_MAX);
        if (mb <= 0) { p++; continue; }
        if (colc < ML_COLS + 7) lines[nlines][colc++] = wc;
        p += mb;
    }
    nlines = (nlines < 256) ? nlines + 1 : 256;
    if (truncated && nlines < 256) {
        lines[nlines][0] = (wchar_t)0x2026; lines[nlines][1] = 0;
        nlines++;
    }
    if (nlines < 1) nlines = 1;

    int rows, cols; getmaxyx(stdscr, rows, cols);
    int pw = (cols < 74) ? cols - 4 : 74;
    int ph = (rows < 16) ? rows - 2 : 14;
    if (pw < 30 || ph < 8) return;
    int px = (cols - pw) / 2;
    int py = (rows - ph) / 2;
    WINDOW *w = newwin(ph, pw, py, px);
    if (!w) return;
    keypad(w, TRUE);

    int innerH = ph - 3;
    int top = 0;
    for (;;) {
        if (top > nlines - innerH) top = nlines - innerH;
        if (top < 0) top = 0;

        werase(w);
        wattron(w,COLOR_PAIR(C_BORDER)|A_BOLD); box(w,0,0); wattroff(w,COLOR_PAIR(C_BORDER)|A_BOLD);
        wattron(w,COLOR_PAIR(C_TOPBAR)|A_BOLD); mvwprintw(w,0,2," %s (view) ",title); wattroff(w,COLOR_PAIR(C_TOPBAR)|A_BOLD);
        wattron(w,A_DIM);
        mvwprintw(w,ph-1,2," line %d-%d of %d   j/k,PgUp/Dn  Esc close ",
            top+1, top+innerH<nlines?top+innerH:nlines, nlines);
        wattroff(w,A_DIM);
        for (int i = 0; i < innerH && top + i < nlines; i++) {
            int len = 0;
            while (len < ML_COLS + 8 && lines[top + i][len]) len++;
            if (len > 0) mvwaddnwstr(w, 1 + i, 2, lines[top + i], len);
        }
        if (top > 0)                 mvwaddch(w, 1, 0, (chtype)'^'|A_DIM);
        if (top + innerH < nlines)   mvwaddch(w, ph-2, 0, (chtype)'v'|A_DIM);
        wrefresh(w);

        wint_t ch;
        int rc = get_wch(&ch);
        if (rc == ERR) continue;
        if (rc == KEY_CODE_YES) {
            switch (ch) {
            case KEY_UP:   if (top > 0) top--; break;
            case KEY_DOWN: if (top + innerH < nlines) top++; break;
            case KEY_PPAGE: top -= innerH - 1; if (top < 0) top = 0; break;
            case KEY_NPAGE: top += innerH - 1; break;
            case KEY_HOME: top = 0; break;
            case KEY_END:  top = nlines - innerH; break;
            case KEY_MOUSE: { MEVENT ev; while (getmouse(&ev)==OK) {} } break;
            case KEY_RESIZE: default: break;
            }
        } else if (ch == 27 || ch == '\n' || ch == '\r' || ch == 'q') {
            break;
        } else if (ch == 'k') {
            if (top > 0) top--;
        } else if (ch == 'j') {
            if (top + innerH < nlines) top++;
        }
    }
    delwin(w);
    touchwin(stdscr); refresh();
}


/* validation callback: return true to accept the input on Enter.
   on false, errbuf receives a message shown inside the popup,
   which stays open for correction. */
typedef bool (*popup_validate_fn)(const char *input, void *udata, char *errbuf, int errsz);

static int popup_impl(const char *title, const char *hint, char *out, int maxlen,
                      popup_validate_fn val, void *udata){
    int rows, cols; getmaxyx(stdscr, rows, cols);
    int pw = (cols < 74) ? cols - 4 : 74;
    if (pw < 20 || rows < 9) return 0;

    int field_w = pw - 7;

    #define PH_FOR(fr) ((fr) + 7)

    #define CLAMP_PH(ph) do { \
        if ((ph) > rows - 2) (ph) = rows - 2; \
        if ((ph) < PH_FOR(1)) (ph) = PH_FOR(1); \
    } while(0)

    int field_rows = popup_needed_rows(out, field_w);
    if (field_rows < 1) field_rows = 1;
    int ph = PH_FOR(field_rows);
    CLAMP_PH(ph);

    int px = (cols - pw) / 2;
    int py = rows/2 - ph/2;
    WINDOW *p = newwin(ph, pw, py, px); if (!p) return 0;
    keypad(p, TRUE);           /* decode arrow keys (else ESC closes the popup) */

    char errbuf[128] = "";

    /* full redraw: chrome + field, bottom bar always at ph-2 (inside border) */
    #define POPUP_REDRAW() do { \
        werase(p); \
        wattron(p,COLOR_PAIR(C_BORDER)|A_BOLD); box(p,0,0); wattroff(p,COLOR_PAIR(C_BORDER)|A_BOLD); \
        wattron(p,COLOR_PAIR(C_TOPBAR)|A_BOLD); mvwprintw(p,0,2," %s ",title); wattroff(p,COLOR_PAIR(C_TOPBAR)|A_BOLD); \
        if(hint&&hint[0]){wattron(p,A_DIM); mvwprintw(p,2,3,"%.*s",pw-6,hint); wattroff(p,A_DIM);} \
        wattron(p,A_DIM); mvwprintw(p,ph-2,3," Enter:confirm   Esc:cancel "); wattroff(p,A_DIM); \
        popup_draw_field(p, 4, ph-3, field_w, out, cur_byte, &cur_row, &cur_col); \
        if(errbuf[0]){ \
            int elen=(int)strlen(errbuf); \
            int ex=(pw-2-elen)/2; if(ex<1)ex=1; \
            wattron(p,COLOR_PAIR(C_PIN)|A_BOLD); \
            mvwprintw(p,ph-3,ex,"%s",errbuf); \
            wattroff(p,COLOR_PAIR(C_PIN)|A_BOLD); \
        } \
    } while(0)

    curs_set(1);
    int len = (int)strnlen(out, (size_t)(maxlen-1));
    int cancelled = 0;
    int cur_byte = len;        /* text cursor (byte offset) */
    int cur_row = 4, cur_col = 5;

    POPUP_REDRAW();
    wmove(p, cur_row, cur_col); wrefresh(p);

    while (1) {
        int ch = wgetch(p);
        if (ch == KEY_F(1)) { out[0]='\0'; cancelled=1; break; }
        if (ch == 27) {
            /* bare Esc cancels; otherwise decode an escape sequence that
               ncurses didn't match (e.g. normal-mode arrow keys) */
            int c1 = ERR, c2 = ERR, c3 = ERR;
            nodelay(p, TRUE);
            c1 = wgetch(p);
            if (c1 != ERR && (c1 == '[' || c1 == 'O')) {
                c2 = wgetch(p);
                if (c1 == '[' && c2 == '3') c3 = wgetch(p);
            }
            nodelay(p, FALSE);
            if (c1 == ERR || c1 == 27) { out[0]='\0'; cancelled=1; break; }
            if (c1 == 'O') {
                if (c2=='C') ch = KEY_RIGHT;
                else if (c2=='D') ch = KEY_LEFT;
                else if (c2=='H') ch = KEY_HOME;
                else if (c2=='F') ch = KEY_END;
                else if (c2=='A') ch = KEY_UP;
                else if (c2=='B') ch = KEY_DOWN;
            } else if (c1 == '[') {
                if (c2=='C') ch = KEY_RIGHT;
                else if (c2=='D') ch = KEY_LEFT;
                else if (c2=='A') ch = KEY_UP;
                else if (c2=='B') ch = KEY_DOWN;
                else if (c2=='H') ch = KEY_HOME;
                else if (c2=='F') ch = KEY_END;
                else if (c2=='3' && c3=='~') ch = KEY_DC;
            }
            if (ch == 27) {
                /* unknown sequence: ignore and keep the popup open */
                POPUP_REDRAW();
                wmove(p, cur_row, cur_col); wrefresh(p);
                continue;
            }
            /* decoded: fall through to normal key handling */
        }
        if (ch == '\n' || ch == '\r') {
            if (val) {
                if (len == 0) { out[0]='\0'; cancelled=1; break; }   /* blank = cancel */
                if (!val(out, udata, errbuf, (int)sizeof errbuf)) {
                    POPUP_REDRAW();                  /* stay open, show error */
                    wmove(p, cur_row, cur_col); wrefresh(p);
                    continue;
                }
            }
            break;                     /* accept */
        }

        /* cursor movement within the field */
        if (ch == KEY_LEFT && cur_byte > 0) {
            cur_byte--;
            while (cur_byte > 0 && ((unsigned char)out[cur_byte] & 0xC0) == 0x80)
                cur_byte--;            /* stay on char boundaries */
        }
        else if (ch == KEY_RIGHT && cur_byte < len) {
            cur_byte++;
            while (cur_byte < len && ((unsigned char)out[cur_byte] & 0xC0) == 0x80)
                cur_byte++;
        }
        else if (ch == KEY_HOME || ch == KEY_BEG) cur_byte = 0;
        else if (ch == KEY_END)       cur_byte = len;
        /* ignore ONLY actual key codes (not ASCII chars) to prevent leaking to main loop */
        else if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_PPAGE || ch == KEY_NPAGE) {
            /* just redraw and keep waiting for valid input */
            POPUP_REDRAW();
            wmove(p, cur_row, cur_col); wrefresh(p);
            continue;
        }

        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b') && cur_byte > 0) {
            /* delete the character before the cursor */
            int i = cur_byte - 1;
            while (i > 0 && ((unsigned char)out[i] & 0xC0) == 0x80) i--;
            memmove(out + i, out + cur_byte, (size_t)(len - cur_byte + 1));
            len -= cur_byte - i;
            cur_byte = i;
            errbuf[0]='\0';            /* new input: clear error */
        }
        else if (ch == KEY_DC && cur_byte < len) {
            /* delete the character under the cursor */
            int i = cur_byte + 1;
            while (i < len && ((unsigned char)out[i] & 0xC0) == 0x80) i++;
            memmove(out + cur_byte, out + i, (size_t)(len - i + 1));
            len -= i - cur_byte;
            errbuf[0]='\0';
        }
        /* accept printable ASCII and all UTF-8 continuation bytes (0x80+) */
        else if ((ch >= 32 && ch <= 126) || (ch & 0x80)) {
            if (len < maxlen-1) {
                /* insert at the cursor, shifting the rest right */
                memmove(out + cur_byte + 1, out + cur_byte, (size_t)(len - cur_byte));
                out[cur_byte] = (char)ch;
                cur_byte++;
                len++;
                out[len] = '\0';
                errbuf[0]='\0';
            }
        }

        /* resize popup if text now wraps to a different number of lines */
        int new_fr = popup_needed_rows(out, field_w);
        if (new_fr < 1) new_fr = 1;
        int new_ph = PH_FOR(new_fr);
        CLAMP_PH(new_ph);

        if (new_ph != ph) {
            ph = new_ph;
            delwin(p);
            py = rows/2 - ph/2;
            p = newwin(ph, pw, py, px);
            if (!p) break;
        }

        POPUP_REDRAW();
        wmove(p, cur_row, cur_col); wrefresh(p);
    }

    #undef POPUP_REDRAW
    #undef PH_FOR
    #undef CLAMP_PH
    curs_set(0); delwin(p); touchwin(stdscr); refresh();
    return (!cancelled && len > 0);
}

static int popup(const char *title, const char *hint, char *out, int maxlen){
    return popup_impl(title, hint, out, maxlen, NULL, NULL);
}

/* validator for schedule-time popups: parses and range-checks the input,
   writes an inline error message when invalid */
typedef struct { time_t when; } SchedVal;

static bool validate_sched_time(const char *in, void *ud, char *errbuf, int errsz){
    SchedVal *sv=(SchedVal*)ud;
    if (!in[0]) { snprintf(errbuf,(size_t)errsz,"! enter a time (Esc cancels)"); return false; }
    if (!parse_time_input(in,&sv->when)) { snprintf(errbuf,(size_t)errsz,"! can't parse that time"); return false; }
    if (sv->when<=time(NULL))            { snprintf(errbuf,(size_t)errsz,"! that time already passed"); return false; }
    return true;
}

/* ------ draw: too small overlay ------ */
static void draw_toosmall(int rows, int cols) {
    erase();
    attron(COLOR_PAIR(C_TOPBAR)|A_BOLD);
    for(int r=0;r<rows;r++) mvhline(r,0,' ',cols);

    static const char *lines[] = {
        "=^..^=",
        "terminal too small~",
        "please resize and bring me some coffe nya!",
};
    int n = 3;
    int sy = rows/2 - n/2; if(sy<0) sy=0;
    for(int i=0;i<n&&sy+i<rows;i++){
        int slen=(int)strlen(lines[i]);
        int sx=(cols-slen)/2; if(sx<0)sx=0;
        mvprintw(sy+i, sx, "%s", lines[i]);
    }
    attroff(COLOR_PAIR(C_TOPBAR)|A_BOLD);
    refresh();
}

/* ------ draw: top bar ------ */
static void draw_top(WINDOW *w,int cols){
    if(!w) return;
    werase(w);
    wattron(w,COLOR_PAIR(C_TOPBAR)|A_BOLD);
    mvwprintw(w,0,0,"%*s",cols,"");
    char title[64];
    const char *tabname = (view==VIEW_TASKS)?"TASKS":(view==VIEW_NOTES)?"NOTES":"SCHED";
    snprintf(title,sizeof title," =^..^=  meowdo // %s(%d) ",tabname,
        view==VIEW_TASKS?todo_count:(view==VIEW_NOTES?note_count:g_sched_c));
    mvwprintw(w,0,2,"%s",title);
    int cx=2+(int)strlen(title)+1;
    if(filter_tag[0]){
        wattron(w,COLOR_PAIR(C_TAG1)|A_BOLD);
        mvwprintw(w,0,cx," #%s ",filter_tag); cx+=(int)strlen(filter_tag)+4;
        wattron(w,COLOR_PAIR(C_TOPBAR)|A_BOLD);
    }
    if(search[0]){
        wattron(w,COLOR_PAIR(C_SEARCH)|A_BOLD);
        mvwprintw(w,0,cx," /%s/ ",search);
        wattron(w,COLOR_PAIR(C_TOPBAR)|A_BOLD);
    }
    time_t now=time(NULL); struct tm tb; struct tm *tm=localtime_r(&now,&tb);
    if(tm){
        char ts[32]; strftime(ts,sizeof ts,"%H:%M  %a %d %b",tm);
        mvwprintw(w,0,cols-(int)strlen(ts)-2,"%s",ts);
    }
    wattroff(w,COLOR_PAIR(C_TOPBAR)|A_BOLD);
    wnoutrefresh(w);
}

/* ------ draw: status bar ------ */
static void draw_sbar(WINDOW *w,int cols){
    if(!w) return;
    werase(w);
    wattron(w,COLOR_PAIR(C_SBAR)|A_BOLD);
    mvwprintw(w,0,0,"%*s",cols,"");
    if(view==VIEW_SCHED){
        if(cols>100)
            mvwprintw(w,0,1," n:new  e:edit  m:desc  v:view  S:resched  u:release  d:del  D:del-all  /:search  Tab:next  q:quit");
        else if(cols>50)
            mvwprintw(w,0,1," n  e  m  v  S  u  d  D  /  Tab  q");
        else
            mvwprintw(w,0,1," n  e  S  u  d  q");
    }
    else if(view==VIEW_NOTES){
        if(cols>96)
            mvwprintw(w,0,1," n:new note  e/m:edit  Enter:open  J/K:move  d:del  D:del-all  /:search  Tab:tasks  q:quit");
        else if(cols>60)
            mvwprintw(w,0,1," n:new  e:edit  v:view  d:del  /:search  Tab:tasks  q:quit");
        else
            mvwprintw(w,0,1," n  e  v  d  D  /  Tab  q");
    }
    else if(cols>124)
        mvwprintw(w,0,1," n:new  e:edit  m:desc  v:view  Spc:done  J/K:move  p:pin  t:tag  S:sched  d:del  D:del-all  /:search  Tab:notes  q:quit");
    else if(cols>100)
        mvwprintw(w,0,1," n:new  e:edit  m:desc  Spc:done  J/K:move  S:sched  d:del  /:search  Tab:notes  q:quit");
    else if(cols>50)
        mvwprintw(w,0,1," n:new  e:edit  m:desc  v:view  Spc:done  d:del  D:all  /:search  Tab:notes  q:quit");
    else
        mvwprintw(w,0,1," n  e  m  v  Spc  d  D  /  Tab  q");
    if(smsg[0]&&smsg_ttl>0){
        int slen=(int)strlen(smsg);
        if(cols-slen-2>2){
            wattron(w,COLOR_PAIR(C_GREEN)|A_BOLD);
            mvwprintw(w,0,cols-slen-2,"%s",smsg);
            wattroff(w,COLOR_PAIR(C_GREEN)|A_BOLD);
        }
    }
    wattroff(w,COLOR_PAIR(C_SBAR)|A_BOLD);
    wnoutrefresh(w);
}

/* ------ draw: progress bar ------ */
static void draw_progress(WINDOW *w,int y,int x,int width){
    if(!w||todo_count==0||width<4) return;
    int filled=(width*g_done_c)/todo_count;
    int pct=(g_done_c*100)/todo_count;
    wattron(w,COLOR_PAIR(C_GREEN)|A_BOLD); mvwprintw(w,y,x,"[");
    for(int i=0;i<width;i++){
        if(i<filled){wattron(w,COLOR_PAIR(C_GREEN)|A_BOLD);waddch(w,'#');}
        else        {wattron(w,COLOR_PAIR(C_DONE)|A_DIM); waddch(w,'-');}
    }
    wattron(w,COLOR_PAIR(C_GREEN)|A_BOLD);
    wprintw(w,"] %d%%  %d/%d",pct,g_done_c,todo_count);
    wattroff(w,A_BOLD|A_DIM);
}

/* ------ draw: celebration overlay ------ */
static void draw_celebrate(int rows,int cols){
    int cp=(celebrate%2)?COLOR_PAIR(C_TOPBAR):COLOR_PAIR(C_GREEN);
    attron(cp|A_BOLD);
    for(int r=1;r<rows-1;r++) mvhline(r,0,' ',cols);
    static const char *lines[]={
        "  =^..^=  =^..^=  =^..^=  ",
        "                           ",
        "   *  ALL TASKS DONE!!  *  ",
        "    purrfect!! nya nya~    ",
        "                           ",
        "  =^..^=  =^..^=  =^..^=  ",
    };
    int n=6, sy=(rows-1)/2-n/2; if(sy<1)sy=1;
    int sx=(cols-27)/2; if(sx<0)sx=0;
    for(int i=0;i<n&&sy+i<rows-1;i++) mvprintw(sy+i,sx,"%s",lines[i]);
    attroff(cp|A_BOLD); refresh();
}

/* ------ draw: left pane ------ */
static void draw_left(WINDOW *w,int h,int lw,int top){
    if(!w) return;
    const bool notes_view = (view==VIEW_NOTES);
    const bool sched_view = (view==VIEW_SCHED);
    const int  items      = notes_view ? note_vis_count
                          : sched_view ? sched_vis_count : vis_count;
    const int  cursel     = notes_view ? sel_note
                          : sched_view ? sel_sched    : sel;

    werase(w);
    wattron(w,COLOR_PAIR(C_BORDER)|A_BOLD); box(w,0,0); wattroff(w,COLOR_PAIR(C_BORDER)|A_BOLD);
    wattron(w,COLOR_PAIR(C_HDR)|A_BOLD);
    mvwprintw(w,0,2,sched_view?"[ SCHEDULED ]":notes_view?"[ NOTES ]":"[ TODO ]");
    wattroff(w,COLOR_PAIR(C_HDR)|A_BOLD);

    if(sched_view){
        /* " N waiting " label */
        char slabel[24]; snprintf(slabel,sizeof slabel," %d waiting ",g_sched_c);
        int slen=(int)strlen(slabel);
        if(lw>slen+12){
            wattron(w,COLOR_PAIR(C_PIN)|A_BOLD);
            mvwprintw(w,0,lw-slen-1,"%s",slabel);
            wattroff(w,COLOR_PAIR(C_PIN)|A_BOLD);
        }
        if(lw > 20){
            wattron(w,A_DIM); mvwprintw(w,2,2,"  due          task"); wattroff(w,A_DIM);
        }
    } else if(notes_view){
        /* " N notes " label */
        char nlabel[24]; snprintf(nlabel,sizeof nlabel," %d notes ",note_count);
        int nlen=(int)strlen(nlabel);
        if(lw>nlen+12){
            wattron(w,COLOR_PAIR(C_PIN)|A_BOLD);
            mvwprintw(w,0,lw-nlen-1,"%s",nlabel);
            wattroff(w,COLOR_PAIR(C_PIN)|A_BOLD);
        }
    } else {
        /* " N left" label — only if it fits */
        char leftlabel[20]; snprintf(leftlabel,sizeof leftlabel," %d left ",g_pend);
        int llen=(int)strlen(leftlabel);
        if(lw>llen+12){
            wattron(w,COLOR_PAIR(C_PIN)|A_BOLD);
            mvwprintw(w,0,lw-llen-1,"%s",leftlabel);
            wattroff(w,COLOR_PAIR(C_PIN)|A_BOLD);
        }

        /* progress bar — only if enough width */
        int pbw = lw - 22;
        if(pbw >= 4) draw_progress(w,1,2,pbw);

        /* column header — only if wide enough */
        if(lw > 20){
            wattron(w,A_DIM); mvwprintw(w,2,2,"  [/] #tag        task"); wattroff(w,A_DIM);
        }
    }
    if(notes_view && lw > 20){
        wattron(w,A_DIM); mvwprintw(w,2,2,"  notes                       "); wattroff(w,A_DIM);
    }
    wattron(w,COLOR_PAIR(C_BORDER)|A_DIM); mvwhline(w,3,1,ACS_HLINE,lw-2); wattroff(w,COLOR_PAIR(C_BORDER)|A_DIM);

    int area=h-LIST_Y0-1; if(area<1)area=1;

    if(items==0){
        wattron(w,COLOR_PAIR(C_BONGO)|A_DIM);
        int ey=h/2;
        if(sched_view){
            mvwprintw(w,ey,  3,search[0]?"  nothing scheduled~ try again?":"  nothing scheduled! press n :3");
            mvwprintw(w,ey+1,3,search[0]?"  press Esc to clear /     ":"  press S on a task to schedule it~");
        } else if(notes_view){
            mvwprintw(w,ey,  3,search[0]?"  no notes match~ try again?":"  no notes yet! press n :3");
            mvwprintw(w,ey+1,3,search[0]?"  press Esc to clear /      ":"  jot something down nya~  ");
        } else {
            mvwprintw(w,ey,  3,search[0]?"  no matches~ try again?":"  nothing yet! press n :3");
            mvwprintw(w,ey+1,3,search[0]?"  press Esc to clear /   ":"  meowdo is waiting nya~ ");
        }
        wattroff(w,COLOR_PAIR(C_BONGO)|A_DIM);
        wnoutrefresh(w); return;
    }

    /* scroll arrows */
    if(top>0){
        wattron(w,COLOR_PAIR(C_SEARCH)|A_BOLD);
        mvwprintw(w,LIST_Y0-1,lw-5,"(^)");
        wattroff(w,COLOR_PAIR(C_SEARCH)|A_BOLD);
    }
    if(top+area<items){
        wattron(w,COLOR_PAIR(C_SEARCH)|A_BOLD);
        mvwprintw(w,h-1,lw-5,"(v)");
        wattroff(w,COLOR_PAIR(C_SEARCH)|A_BOLD);
    }

    for(int i=0;i<area;i++){
        int vi=top+i, row=LIST_Y0+i;
        if(vi>=items){ mvwprintw(w,row,1,"%*s",lw-2,""); continue; }
        int is_sel=(vi==cursel);

        if(sched_view){
            Todo *t=&todos[sched_vis[vi]];
            wattrset(w,A_NORMAL);
            if(is_sel){
                wattron(w,COLOR_PAIR(C_SEL)|A_BOLD);
                mvwprintw(w,row,1,"%*s",lw-2,"");
            }
            wattron(w,is_sel?COLOR_PAIR(C_SEL)|A_BOLD:COLOR_PAIR(C_BONGO)|A_BOLD);
            char cd[32];
            fmt_in(t->sched_at,cd,(int)sizeof cd);
            mvwprintw(w,row,2,"%9s ",cd);
            int avail=lw-14; if(avail<1)avail=1;
            wattron(w,is_sel?COLOR_PAIR(C_SEL)|A_BOLD:COLOR_PAIR(C_PEND)|A_BOLD);
            char clipped[MAX_LINE];
            int w_used=trunc_to_width(t->text,clipped,sizeof clipped,avail);
            waddstr(w,clipped);
            for(int p2=0;p2<avail-w_used;p2++) waddch(w,' ');
            wattrset(w,A_NORMAL);
            continue;
        }

        if(notes_view){
            Note *n=&notes[note_vis[vi]];
            wattrset(w,A_NORMAL);
            if(is_sel){
                wattron(w,COLOR_PAIR(C_SEL)|A_BOLD);
                mvwprintw(w,row,1,"%*s",lw-2,"");
            }
            /* only the first line of a multiline note — a raw '\n' would
               spill over the pane border */
            const char *nl=strchr(n->text,'\n');
            size_t tlen = nl ? (size_t)(nl - n->text) : strlen(n->text);
            char title[512];
            if (tlen >= sizeof title) tlen = sizeof title - 1;
            memcpy(title, n->text, tlen);
            title[tlen] = '\0';
            bool more = (nl != NULL);
            int avail=lw-8; if(avail<1)avail=1;
            wattron(w,is_sel?COLOR_PAIR(C_SEL)|A_BOLD:COLOR_PAIR(C_PEND)|A_BOLD);
            mvwprintw(w,row,2,"  ");
            char clipped[512];
            int w_used=trunc_to_width(title,clipped,(int)sizeof clipped,
                                      avail - (more?2:0));
            waddstr(w,clipped);
            if (more) waddstr(w," …");
            for(int p2=w_used+(more?2:0);p2<avail;p2++) waddch(w,' ');
            wattrset(w,A_NORMAL);
            continue;
        }

        Todo *t=&todos[vis[vi]];

        wattrset(w,A_NORMAL);
        if(is_sel){
            wattron(w,COLOR_PAIR(C_SEL)|A_BOLD);
            mvwprintw(w,row,1,"%*s",lw-2,"");
        }

        char mark=(t->pinned&&!t->done)?'*':' ';
        char box_ch=t->done?'x':' ';
        char dmark=t->desc[0]?'|':' ';
        int col_attr;
        if      (is_sel)    col_attr=COLOR_PAIR(C_SEL)|A_BOLD;
        else if (t->done)   col_attr=COLOR_PAIR(C_DONE)|A_DIM;
        else if (t->pinned) col_attr=COLOR_PAIR(C_PIN)|A_BOLD;
        else                col_attr=COLOR_PAIR(C_PEND)|A_BOLD;
        wattron(w,col_attr); mvwprintw(w,row,2,"%c [%c]",mark,box_ch);

        /* tag pill — only if enough room */
        int tx=8;
        if(t->tag[0]&&strcmp(t->tag,"none")!=0 && lw>tx+12){
            int tc=tag_color(t->tag), tcnt=tag_cnt_for[vis[vi]];
            if(!is_sel){wattrset(w,A_NORMAL);wattron(w,COLOR_PAIR(tc)|A_BOLD);}
            mvwprintw(w,row,tx,"#%.*s(%d)",5,t->tag,tcnt);
            tx+=9;
        }

        /* description indicator */
        if(t->desc[0]&&lw>tx+2){
            wattron(w,is_sel?COLOR_PAIR(C_SEL)|A_BOLD:COLOR_PAIR(C_BONGO)|A_DIM);
            mvwprintw(w,row,tx,"%c",dmark);
            tx+=2;
        }

        int avail=lw-tx-4; if(avail<1)avail=1;
        if     (is_sel)   {wattron(w,COLOR_PAIR(C_SEL)|A_BOLD);}
        else if(t->done)  {wattrset(w,A_NORMAL);wattron(w,COLOR_PAIR(C_DONE)|A_DIM);}
        else if(t->pinned){wattrset(w,A_NORMAL);wattron(w,COLOR_PAIR(C_PIN));}
        else              {wattrset(w,A_NORMAL);wattron(w,COLOR_PAIR(C_PEND));}
        /* safely truncate multibyte text to display width */
        char clipped[MAX_LINE];
        int w_used=trunc_to_width(t->text,clipped,sizeof clipped,avail);
        mvwaddch(w,row,tx,' ');
        waddstr(w,clipped);
        /* pad remaining space */
        for(int p=0;p<avail-w_used;p++) waddch(w,' ');
        wattrset(w,A_NORMAL);
    }
    wnoutrefresh(w);
}

/* draw a detail line padded to the pane width, so a shorter redraw
   never leaves stale cells behind */
static void pane_line(WINDOW *w, int row, int pane_w, const char *text) {
    mvwprintw(w, row, 3, "%-*s", pane_w - 5, text);
}

/* ------ draw: right pane ------ */
static void draw_right(WINDOW *w,int h,int rw){
    if(!w) return;
    werase(w);
    wattron(w,COLOR_PAIR(C_BORDER)|A_DIM); box(w,0,0); wattroff(w,COLOR_PAIR(C_BORDER)|A_DIM);
    int ww=getmaxx(w);

    /* bongo cat — only draw if right pane is wide enough */
    int bx = ww - 20;
    if(bx >= 1 && rw >= 22){
        wattron(w,COLOR_PAIR(C_BONGO)|A_BOLD);
        for(int i=0;i<BC_H&&i+1<h-2;i++) mvwprintw(w,i+1,bx,"%s",BONGO[i]);
        wattroff(w,COLOR_PAIR(C_BONGO)|A_BOLD);
    }

    int ca = (bx >= 1 && rw >= 22) ? bx-3 : ww-4;
    if(ca < 4){ wnoutrefresh(w); return; }

    wattron(w,COLOR_PAIR(C_HDR)|A_BOLD); mvwprintw(w,0,2,"[ MEOWDO ]"); wattroff(w,COLOR_PAIR(C_HDR)|A_BOLD);

    /* stats */
    wattron(w,COLOR_PAIR(C_PEND)|A_BOLD);  mvwprintw(w,2,2," * pending : %d",g_pend);    wattroff(w,COLOR_PAIR(C_PEND)|A_BOLD);
    wattron(w,COLOR_PAIR(C_GREEN)|A_BOLD); mvwprintw(w,3,2," x done    : %d",g_done_c);  wattroff(w,COLOR_PAIR(C_GREEN)|A_BOLD);
    wattron(w,COLOR_PAIR(C_PIN)|A_BOLD);   mvwprintw(w,4,2," ^ pinned  : %d",g_pin_c);   wattroff(w,COLOR_PAIR(C_PIN)|A_BOLD);
    wattron(w,A_DIM);                      mvwprintw(w,5,2," # total   : %d",todo_count); wattroff(w,A_DIM);

    /* mini progress */
    if(todo_count>0){
        int bw=ca-8; if(bw>20)bw=20; if(bw<4)bw=4;
        wattron(w,COLOR_PAIR(C_BORDER)|A_DIM); mvwhline(w,6,2,ACS_HLINE,ca); wattroff(w,COLOR_PAIR(C_BORDER)|A_DIM);
        int filled=(bw*g_done_c)/todo_count;
        wattron(w,COLOR_PAIR(C_GREEN)|A_BOLD); mvwprintw(w,7,2,"[");
        for(int i=0;i<bw;i++){
            if(i<filled){wattron(w,COLOR_PAIR(C_GREEN)|A_BOLD);waddch(w,'#');}
            else        {wattron(w,COLOR_PAIR(C_DONE)|A_DIM); waddch(w,'.');}
        }
        wattron(w,COLOR_PAIR(C_GREEN)|A_BOLD);
        wprintw(w,"] %d%%",(g_done_c*100)/todo_count);
        wattroff(w,A_BOLD|A_DIM);
    }

    /* tags */
    int ty=9;
    if(utag_n>0){
        wattron(w,COLOR_PAIR(C_BORDER)|A_DIM); mvwhline(w,ty,2,ACS_HLINE,ca); wattroff(w,COLOR_PAIR(C_BORDER)|A_DIM); ty++;
        wattron(w,COLOR_PAIR(C_HDR)|A_BOLD); mvwprintw(w,ty++,2,"[ TAGS  (1-6) ]"); wattroff(w,COLOR_PAIR(C_HDR)|A_BOLD);
        for(int i=0;i<utag_n&&i<6&&ty<h-5;i++){
            int tc=tag_color(utags[i]);
            int active=(filter_tag[0]&&strcmp(utags[i],filter_tag)==0);
            if(active){
                wattron(w,COLOR_PAIR(C_SEL)|A_BOLD);
                mvwprintw(w,ty++,2," %d > #%-8.8s %3d",i+1,utags[i],utag_counts[i]);
                wattroff(w,COLOR_PAIR(C_SEL)|A_BOLD);
            } else {
                wattron(w,COLOR_PAIR(tc)|A_DIM);
                mvwprintw(w,ty++,2," %d   #%-8.8s %3d",i+1,utags[i],utag_counts[i]);
                wattroff(w,COLOR_PAIR(tc)|A_DIM);
            }
        }
        ty++;
    }

    /* selected task detail / note detail */
    if(view==VIEW_SCHED){
        int svis = sched_vis_count;
        if(svis>0&&sel_sched<svis&&ty<h-3){
            Todo *t=&todos[sched_vis[sel_sched]];
            wattron(w,COLOR_PAIR(C_BORDER)|A_DIM); mvwhline(w,ty,2,ACS_HLINE,ca); wattroff(w,COLOR_PAIR(C_BORDER)|A_DIM); ty++;
            wattron(w,COLOR_PAIR(C_HDR)|A_BOLD); mvwprintw(w,ty++,2,"[ SCHEDULED ]"); wattroff(w,COLOR_PAIR(C_HDR)|A_BOLD);

            bool trunc=false;
            wattron(w,COLOR_PAIR(C_PEND)|A_BOLD);
            ty=draw_wrapped(w,ty,h-4,ca-2,3,t->text,&trunc)+1;
            wattroff(w,COLOR_PAIR(C_PEND)|A_BOLD);

            /* description preview */
            if(t->desc[0]&&ty<h-3){
                bool dtrunc=false;
                wattron(w,COLOR_PAIR(C_HDR)|A_BOLD); mvwprintw(w,ty++,2,"[ DESC ]"); wattroff(w,COLOR_PAIR(C_HDR)|A_BOLD);
                wattron(w,COLOR_PAIR(C_PEND)|A_DIM);
                ty=draw_wrapped(w,ty,h-4,ca-2,3,t->desc,&dtrunc);
                wattroff(w,COLOR_PAIR(C_PEND)|A_DIM);
                if(dtrunc&&ty<h-2){
                    wattron(w,A_DIM); mvwprintw(w,ty++,3,"… v: view full"); wattroff(w,A_DIM);
                } else if(!dtrunc&&ty<h-2){
                    ty++;
                }
            }

            char tsbuf[32], cdbuf[32], linebuf[128];
            fmt_ts(t->sched_at,tsbuf,(int)sizeof tsbuf);
            fmt_in(t->sched_at,cdbuf,(int)sizeof cdbuf);
            if(ty<h-2){
                snprintf(linebuf,sizeof linebuf,"-> %s  (in %s)",tsbuf,cdbuf);
                wattron(w,COLOR_PAIR(C_PIN)|A_BOLD);
                pane_line(w,ty,ww,linebuf); ty++;
                wattroff(w,COLOR_PAIR(C_PIN)|A_BOLD);
            }
            fmt_ts(t->created_at,tsbuf,(int)sizeof tsbuf);
            if(ty<h-2){
                snprintf(linebuf,sizeof linebuf,"+ %s",tsbuf);
                wattron(w,A_DIM); pane_line(w,ty,ww,linebuf); ty++; wattroff(w,A_DIM);
            }
            if(ty<h-2){
                wattron(w,A_DIM); pane_line(w,ty,ww,"u: release now"); ty++; wattroff(w,A_DIM);
            }
        }
    }
    else if(view==VIEW_NOTES){
        int nvis = note_vis_count;
        if(nvis>0&&sel_note<nvis&&ty<h-3){
            Note *n=&notes[note_vis[sel_note]];
            wattron(w,COLOR_PAIR(C_BORDER)|A_DIM); mvwhline(w,ty,2,ACS_HLINE,ca); wattroff(w,COLOR_PAIR(C_BORDER)|A_DIM); ty++;
            wattron(w,COLOR_PAIR(C_HDR)|A_BOLD); mvwprintw(w,ty++,2,"[ NOTE ]"); wattroff(w,COLOR_PAIR(C_HDR)|A_BOLD);

            bool trunc=false;
            wattron(w,COLOR_PAIR(C_PEND)|A_BOLD);
            ty=draw_wrapped(w,ty,h-4,ca-2,3,n->text,&trunc)+1;
            wattroff(w,COLOR_PAIR(C_PEND)|A_BOLD);
            if(trunc&&ty<h-2){
                wattron(w,A_DIM); pane_line(w,ty,ww,"… v: view full"); ty++; wattroff(w,A_DIM);
            }
            char tsbuf[32], linebuf[64];
            fmt_ts(n->created_at,tsbuf,(int)sizeof tsbuf);
            if(ty<h-2){
                snprintf(linebuf,sizeof linebuf,"+ %s",tsbuf);
                wattron(w,A_DIM); pane_line(w,ty,ww,linebuf); ty++; wattroff(w,A_DIM);
            }
        }
    }
    else if(vis_count>0&&sel<vis_count&&ty<h-3){
        Todo *t=&todos[vis[sel]];
        wattron(w,COLOR_PAIR(C_BORDER)|A_DIM); mvwhline(w,ty,2,ACS_HLINE,ca); wattroff(w,COLOR_PAIR(C_BORDER)|A_DIM); ty++;
        wattron(w,COLOR_PAIR(C_HDR)|A_BOLD); mvwprintw(w,ty++,2,"[ SELECTED ]"); wattroff(w,COLOR_PAIR(C_HDR)|A_BOLD);

        /* word-wrap with multibyte safety and ellipsis for truncation */
        bool trunc=false;
        wattron(w,COLOR_PAIR(C_PEND)|A_BOLD);
        ty=draw_wrapped(w,ty,h-4,ca-2,3,t->text,&trunc)+1;
        wattroff(w,COLOR_PAIR(C_PEND)|A_BOLD);

        /* description preview */
        if(t->desc[0]&&ty<h-3){
            bool dtrunc=false;
            wattron(w,COLOR_PAIR(C_BORDER)|A_DIM); mvwhline(w,ty,2,ACS_HLINE,ca); wattroff(w,COLOR_PAIR(C_BORDER)|A_DIM); ty++;
            wattron(w,COLOR_PAIR(C_HDR)|A_BOLD); mvwprintw(w,ty++,2,"[ DESC ]"); wattroff(w,COLOR_PAIR(C_HDR)|A_BOLD);
            wattron(w,COLOR_PAIR(C_PEND)|A_DIM);
            ty=draw_wrapped(w,ty,h-4,ca-2,3,t->desc,&dtrunc);
            wattroff(w,COLOR_PAIR(C_PEND)|A_DIM);
            if(dtrunc&&ty<h-2){
                wattron(w,A_DIM); mvwprintw(w,ty++,3,"… v: view full"); wattroff(w,A_DIM);
            } else if(!dtrunc&&ty<h-2){
                ty++;   /* blank separator */
            }
        }

        if(t->pinned&&ty<h-3){
            wattron(w,COLOR_PAIR(C_PIN)|A_DIM); pane_line(w,ty,ww,"* pinned"); ty++; wattroff(w,COLOR_PAIR(C_PIN)|A_DIM);
        }
        char tsbuf[32], linebuf[64];
        fmt_ts(t->created_at,tsbuf,(int)sizeof tsbuf);
        if(ty<h-2){
            snprintf(linebuf,sizeof linebuf,"+ %s",tsbuf);
            wattron(w,A_DIM); pane_line(w,ty,ww,linebuf); ty++; wattroff(w,A_DIM);
        }
        if(t->done_at&&ty<h-2){
            fmt_ts(t->done_at,tsbuf,(int)sizeof tsbuf);
            snprintf(linebuf,sizeof linebuf,"v %s",tsbuf);
            wattron(w,COLOR_PAIR(C_GREEN)|A_DIM);
            pane_line(w,ty,ww,linebuf); ty++;
            wattroff(w,COLOR_PAIR(C_GREEN)|A_DIM);
        }
    }

    /* mood pinned to bottom */
    {
        const char *mood=bongo_mood(g_pend);
        int mlen=(int)strlen(mood), mx=ww-mlen-1; if(mx<1)mx=1;
        wattron(w,COLOR_PAIR(C_BONGO)|A_BOLD);
        mvwprintw(w,h-1,mx,"%s",mood);
        wattroff(w,COLOR_PAIR(C_BONGO)|A_BOLD);
    }
    wnoutrefresh(w);
}

static int get_left_window_width(int cols){
    #define LEFT_WINDOW_MIN_WIDTH 20
    int lw=cols*58/100; if(lw<LEFT_WINDOW_MIN_WIDTH)lw=LEFT_WINDOW_MIN_WIDTH;
    return lw;
}

/* ------ window allocation helper ------ */
typedef struct {
    WINDOW *top, *left, *rite, *sbar;
} Windows;

static void free_windows(Windows *w){
    if(w->top)  { delwin(w->top);  w->top=NULL;  }
    if(w->left) { delwin(w->left); w->left=NULL; }
    if(w->rite) { delwin(w->rite); w->rite=NULL; }
    if(w->sbar) { delwin(w->sbar); w->sbar=NULL; }
}

static bool alloc_windows(Windows *w, int rows, int cols, int lw, int rw, int ch, Layout layout){
    free_windows(w);
    w->top  = newwin(1,   cols, 0,      0);
    w->sbar = newwin(1,   cols, rows-1, 0);
    if(layout == LAYOUT_FULL){
        w->left = newwin(ch, lw,   1,  0);
        w->rite = newwin(ch, rw,   1, lw);
        return (w->top && w->left && w->rite && w->sbar);
    } else {
        /* compact: left pane takes full width */
        w->left = newwin(ch, cols, 1, 0);
        w->rite = NULL;
        return (w->top && w->left && w->sbar);
    }
}

/* edit the selected note with the multiline editor
   (shared by e, m and Enter in the NOTES tab) */
static void edit_selected_note(void) {
    if(note_vis_count==0) return;
    Note *n=&notes[note_vis[sel_note]];
    if(popup_edit_ml("Edit note",n->text,MAX_NOTE)){
        todos_save(); rebuild_note_vis(); set_smsg("note updated! nya~");
    }
    /* flush any queued input from editor interaction */
    timeout(0); while(getch()!=ERR); timeout(-1);
}

static void print_version(void) {
    printf("meowdo version 1.5.0\n");
    printf("thanks for using meowdo :3\n");
    printf(" =^..^=  =^..^=  =^..^=\n");
}


static void print_help(void) {
    printf("meowdo  ---  a cute bongo cat todo list  =^..^=\n\n");
    printf("Usage: meowdo [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  --version    Show version information\n");
    printf("  --help       Show this help message\n\n");
    printf("Keys:\n");
    printf("  Tab          Cycle TASKS / NOTES / SCHEDULED (Shift-Tab back)\n");
    printf("  j/k          Navigate up/down\n");
    printf("  J/K          Move selected task/note down/up\n");
    printf("  g/G          Jump to top/bottom\n");
    printf("  n            New task / new note / new scheduled task\n");
    printf("  e            Edit selected task / note\n");
    printf("  m            Edit task description; edit note in NOTES tab\n");
    printf("  Enter        Open memo (edit if empty, view otherwise)\n");
    printf("  v            View description / note full-screen\n");
    printf("  Space        Toggle done/not done\n");
    printf("  p            Toggle pin\n");
    printf("  t            Set/clear tag\n");
    printf("  S            Schedule task (hidden until then)\n");
    printf("  u            Release scheduled task now\n");
    printf("  d            Delete selected task / note\n");
    printf("  D            Delete ALL tasks / notes / scheduled\n");
    printf("  /            Search\n");
    printf("  Esc          Clear search + filter\n");
    printf("  1-6          Filter by tag\n");
    printf("  0            Show all\n");
    printf("  q            Quit\n");
}

static bool parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || 
            strcmp(argv[i], "-v") == 0) {
            print_version();
            return true;
        }
        else if (strcmp(argv[i], "--help") == 0 || 
                 strcmp(argv[i], "-h") == 0) {
            print_help();
            return true;
        }
        else {
            printf("Unknown option: %s\n", argv[i]);
            printf("Try 'meowdo --help' for more information.\n");
            return true;
        }
    }
    return false;
}

/* ------ main ------ */
int main(int argc, char *argv[]){
    
    if (parse_args(argc, argv)) {
        return 0; /* Exit after displaying info */
    }

    setlocale(LC_ALL,"");
    setenv("ESCDELAY","25",0);   /* snappy Esc detection (respects user env) */
    g_utf8 = detect_utf8();
    BONGO   = g_utf8 ? BONGO_UTF8 : BONGO_ASCII;
    build_paths();
    initscr(); 
    cbreak(); noecho(); curs_set(0);
    keypad(stdscr,TRUE);
    if(!has_colors()){endwin();puts("need color terminal");return 1;}
    start_color(); use_default_colors();

    init_pair(C_BORDER, COLOR_CYAN,    -1);
    init_pair(C_TOPBAR, COLOR_BLACK,   COLOR_MAGENTA);
    init_pair(C_SEL,    COLOR_BLACK,   COLOR_CYAN);
    init_pair(C_DONE,   COLOR_WHITE,   -1);
    init_pair(C_PEND,   COLOR_YELLOW,  -1);
    init_pair(C_BONGO,  COLOR_MAGENTA, -1);
    init_pair(C_SBAR,   COLOR_BLACK,   COLOR_BLUE);
    init_pair(C_HDR,    COLOR_CYAN,    -1);
    init_pair(C_GREEN,  COLOR_GREEN,   -1);
    init_pair(C_PIN,    COLOR_RED,     -1);
    init_pair(C_TAG1,   COLOR_GREEN,   -1);
    init_pair(C_TAG2,   COLOR_YELLOW,  -1);
    init_pair(C_TAG3,   COLOR_CYAN,    -1);
    init_pair(C_TAG4,   COLOR_MAGENTA, -1);
    init_pair(C_TAG5,   COLOR_RED,     -1);
    init_pair(C_TAG6,   COLOR_BLUE,    -1);
    init_pair(C_TAG7,   COLOR_WHITE,   -1);
    init_pair(C_TAG8,   COLOR_GREEN,   COLOR_BLUE);
    init_pair(C_SEARCH, COLOR_BLACK,   COLOR_YELLOW);
    init_pair(C_BEAT,   COLOR_BLACK,   COLOR_YELLOW);
    init_pair(C_TITLE,  COLOR_BLACK,   COLOR_CYAN);

    if (!todos_load() && todos_import_legacy())
        todos_save();   /* one-time migration from legacy todos.txt */
    if (process_reveals()) todos_save();   /* reveal anything already due */
    rebuild_vis(); rebuild_note_vis(); rebuild_sched_vis(); rebuild_tag_cache();
    erase(); refresh();

    int rows, cols; getmaxyx(stdscr, rows, cols);
    Layout layout = get_layout(rows, cols);
    int lw = get_left_window_width(cols);
    int rw = cols - lw;
    int ch = rows - 2;

    Windows win = {NULL,NULL,NULL,NULL};
    if(layout != LAYOUT_TOOSMALL){
        alloc_windows(&win, rows, cols, lw, rw, ch, layout);
    }

    int list_top = 0;
    bool running = true;

    while(running){
        int nr, nc; getmaxyx(stdscr, nr, nc);
        Layout new_layout = get_layout(nr, nc);

        if(nr!=rows || nc!=cols || new_layout!=layout){
            rows=nr; cols=nc; layout=new_layout;
            lw = get_left_window_width(cols);
            rw = cols - lw;
            ch = rows - 2; if(ch < 1) ch = 1;
            free_windows(&win);
            erase(); refresh();
            if(layout != LAYOUT_TOOSMALL){
                alloc_windows(&win, rows, cols, lw, rw, ch, layout);
            }
        }

        if(layout == LAYOUT_TOOSMALL){
            draw_toosmall(rows, cols);
            timeout(200);
            int key = getch();
            if(key == 'q') running = false;
            continue;
        }

        if(tags_dirty) rebuild_tag_cache();
        if(process_reveals()){
            todos_save();
            rebuild_vis(); rebuild_sched_vis();
            set_smsg("scheduled task arrived! nya~");
        }
        recount();

        int pane_w = (layout == LAYOUT_FULL) ? lw : cols;
        int area = ch - LIST_Y0 - 1; if(area<1)area=1;
        int *top_p = (view==VIEW_TASKS) ? &list_top
                   : (view==VIEW_NOTES) ? &note_top : &sched_top;
        int cursel = (view==VIEW_TASKS) ? sel
                   : (view==VIEW_NOTES) ? sel_note : sel_sched;
        if(cursel<*top_p)       *top_p=cursel;
        if(cursel>=*top_p+area) *top_p=cursel-area+1;
        if(*top_p<0)            *top_p=0;

        draw_top (win.top,  cols);
        draw_left(win.left, ch, pane_w, *top_p);
        if(layout == LAYOUT_FULL && win.rite)
            draw_right(win.rite, ch, rw);
        draw_sbar(win.sbar, cols);

        if(celebrate>0){ draw_celebrate(rows,cols); celebrate--; }

        doupdate();

        /* wake up every 30s so scheduled tasks appear on time */
        timeout(celebrate>0 ? 150 : 30000);
        if(smsg_ttl>0) smsg_ttl--;

        int key=getch();
        if(key==ERR) continue;
        smsg[0]='\0';

        switch(key){
        case 'q': running=false; break;

        case '\t': case KEY_BTAB:
            view = (View)(((int)view + ((key=='\t')?1:2)) % 3);
            list_top=0; note_top=0; sched_top=0;
            if(view==VIEW_TASKS)      rebuild_vis();
            else if(view==VIEW_NOTES) rebuild_note_vis();
            else                      rebuild_sched_vis();
            set_smsg(view==VIEW_TASKS?"tasks tab":(view==VIEW_NOTES?"notes tab":"scheduled tab"));
            break;

        case KEY_UP: case 'k':
            if(view==VIEW_SCHED){ if(sel_sched>0) sel_sched--; }
            else if(view==VIEW_NOTES){ if(sel_note>0) sel_note--; }
            else if(sel>0) sel--;
            break;
        case KEY_DOWN: case 'j':
            if(view==VIEW_SCHED){ if(sel_sched<sched_vis_count-1) sel_sched++; }
            else if(view==VIEW_NOTES){ if(sel_note<note_vis_count-1) sel_note++; }
            else if(sel<vis_count-1) sel++;
            break;
        case 'g': case KEY_HOME:
            if(view==VIEW_SCHED) sel_sched=0;
            else if(view==VIEW_NOTES) sel_note=0;
            else sel=0;
            break;
        case 'G': case KEY_END:
            if(view==VIEW_SCHED) sel_sched=sched_vis_count>0?sched_vis_count-1:0;
            else if(view==VIEW_NOTES) sel_note=note_vis_count>0?note_vis_count-1:0;
            else sel=vis_count>0?vis_count-1:0;
            break;
        case KEY_NPAGE:
            if(view==VIEW_SCHED){ sel_sched+=area/2; if(sel_sched>=sched_vis_count) sel_sched=sched_vis_count>0?sched_vis_count-1:0; }
            else if(view==VIEW_NOTES){ sel_note+=area/2; if(sel_note>=note_vis_count) sel_note=note_vis_count>0?note_vis_count-1:0; }
            else { sel+=area/2; if(sel>=vis_count) sel=vis_count>0?vis_count-1:0; }
            break;
        case KEY_PPAGE:
            if(view==VIEW_SCHED){ sel_sched-=area/2; if(sel_sched<0) sel_sched=0; }
            else if(view==VIEW_NOTES){ sel_note-=area/2; if(sel_note<0) sel_note=0; }
            else { sel-=area/2; if(sel<0) sel=0; }
            break;

        case 'J': case 'K':{
            /* move the selected item down/up in the list */
            int dir=(key=='J')?1:-1;
            if(view==VIEW_SCHED) break;   /* scheduled list is time-ordered */
            if(view==VIEW_NOTES){
                if(note_vis_count<2) break;
                int cur=sel_note, nxt=cur+dir;
                if(nxt<0||nxt>=note_vis_count) break;
                int a=note_vis[cur], b=note_vis[nxt];
                Note nt=notes[a]; notes[a]=notes[b]; notes[b]=nt;
                todos_save(); rebuild_note_vis();
                sel_note=nxt;
                set_smsg(dir>0?"moved down~":"moved up!");
            } else {
                if(vis_count<2) break;
                int cur=sel, nxt=cur+dir;
                if(nxt<0||nxt>=vis_count) break;
                int a=vis[cur], b=vis[nxt];
                /* can't reorder across the pinned/done groups */
                if(todo_class(&todos[a])!=todo_class(&todos[b])){
                    set_smsg("can't move past pinned/done~");
                    break;
                }
                Todo tt=todos[a]; todos[a]=todos[b]; todos[b]=tt;
                todos_save(); rebuild_vis();
                sel=nxt;
                set_smsg(dir>0?"moved down~":"moved up!");
            }
            break;
        }

        case 'n':{
            if(view==VIEW_SCHED){
                char buf[MAX_LINE]="";
                if(popup("New scheduled task","what needs doing?",buf,(int)sizeof buf)){
                    char tbuf[MAX_LINE]="", hint[128];
                    sched_hint(hint,(int)sizeof hint);
                    SchedVal sv;
                    if(popup_impl("Schedule",hint,tbuf,(int)sizeof tbuf,validate_sched_time,&sv)){
                        time_t when=sv.when;
                        if(todo_count<MAX_TODOS){
                            Todo *t=&todos[todo_count];
                            memset(t,0,sizeof *t);
                            snprintf(t->tag,MAX_TAG,"none");
                            snprintf(t->text,MAX_LINE,"%s",buf);
                            t->created_at=time(NULL);
                            t->sched_at=when;
                            todo_count++; todos_save();
                            rebuild_vis(); rebuild_sched_vis();
                            sel_sched=sched_vis_count-1;
                            char m[96],tsbuf[32];
                            fmt_ts(when,tsbuf,(int)sizeof tsbuf);
                            snprintf(m,sizeof m,"will appear %s~",tsbuf);
                            set_smsg(m);
                        }
                    }
                }
            }
            else if(view==VIEW_NOTES){
                char buf[MAX_NOTE]="";
                if(popup_edit_ml("New note",buf,sizeof buf)&&note_count<MAX_NOTES){
                    Note *nt=&notes[note_count];
                    snprintf(nt->text,MAX_NOTE,"%s",buf);
                    nt->created_at=time(NULL);
                    note_count++; todos_save(); rebuild_note_vis();
                    sel_note=note_vis_count-1;
                    set_smsg("note added! nya~");
                }
            } else {
                char buf[MAX_LINE]="";
                if(popup("New task","what needs doing?",buf,(int)sizeof buf)&&todo_count<MAX_TODOS){
                    Todo *t=&todos[todo_count];
                    t->done=false;t->pinned=false;t->created_at=time(NULL);t->done_at=0;
                    snprintf(t->tag,  MAX_TAG,  "%s", filter_tag[0]?filter_tag:"none");
                    snprintf(t->text, MAX_LINE, "%s", buf);
                    todo_count++; todos_save(); rebuild_vis(); sel=vis_count-1;
                    set_smsg("task added! nya~");
                }
            }
            /* flush any queued input from popup interaction */
            timeout(0); while(getch()!=ERR); timeout(-1);
            break;
        }

        case 'e':{
            if(view==VIEW_NOTES){
                edit_selected_note();
            } else {
                if((view==VIEW_SCHED?sched_vis_count:vis_count)==0) break;
                int ti = (view==VIEW_SCHED) ? sched_vis[sel_sched] : vis[sel];
                Todo *t=&todos[ti];
                char buf[MAX_LINE];
                snprintf(buf, MAX_LINE, "%s", t->text);
                if(popup("Edit task","edit text  (Esc=cancel)",buf,(int)sizeof buf)){
                    snprintf(t->text, MAX_LINE, "%s", buf);
                    todos_save();
                    if(view==VIEW_SCHED) rebuild_sched_vis(); else rebuild_vis();
                    set_smsg("task updated! nya~");
                }
                /* flush any queued input from popup interaction */
                timeout(0); while(getch()!=ERR); timeout(-1);
            }
            break;
        }

        case 'v':{
            if(view==VIEW_NOTES){
                if(note_vis_count==0) break;
                Note *n=&notes[note_vis[sel_note]];
                if(!n->text[0]) { set_smsg("empty note (press e to write)"); break; }
                view_multiline("NOTE", n->text);
            } else if(view==VIEW_SCHED){
                if(sched_vis_count==0) break;
                Todo *t=&todos[sched_vis[sel_sched]];
                if(!t->desc[0]) { set_smsg("no description (press m to add)"); break; }
                view_multiline("DESCRIPTION", t->desc);
            } else {
                if(vis_count==0) break;
                Todo *t=&todos[vis[sel]];
                if(!t->desc[0]) { set_smsg("no description (press m to add)"); break; }
                view_multiline("DESCRIPTION", t->desc);
            }
            /* flush any queued input from editor interaction */
            timeout(0); while(getch()!=ERR); timeout(-1);
            break;
        }

        case 'm':{
            if(view==VIEW_NOTES){
                edit_selected_note();
            }
            else if(view==VIEW_SCHED && sched_vis_count>0){
                Todo *t=&todos[sched_vis[sel_sched]];
                if(popup_edit_ml("Task description",t->desc,sizeof t->desc)){
                    todos_save(); set_smsg("description saved! nya~");
                }
                /* flush any queued input from editor interaction */
                timeout(0); while(getch()!=ERR); timeout(-1);
            }
            else if(view==VIEW_TASKS && vis_count>0){
                Todo *t=&todos[vis[sel]];
                if(popup_edit_ml("Task description",t->desc,sizeof t->desc)){
                    todos_save(); set_smsg("description saved! nya~");
                }
                /* flush any queued input from editor interaction */
                timeout(0); while(getch()!=ERR); timeout(-1);
            }
            break;
        }

        case '\n': case '\r': case KEY_ENTER:{
            /* Enter opens the memo for the selected item:
               edit if empty, view full-screen otherwise */
            if(view==VIEW_NOTES){
                if(note_vis_count==0) break;
                Note *n=&notes[note_vis[sel_note]];
                if(n->text[0]) view_multiline("NOTE", n->text);
                else           edit_selected_note();
            } else if(view==VIEW_SCHED){
                if(sched_vis_count==0) break;
                Todo *t=&todos[sched_vis[sel_sched]];
                if(t->desc[0]) view_multiline("DESCRIPTION", t->desc);
                else if(popup_edit_ml("Task description",t->desc,sizeof t->desc)){
                    todos_save(); set_smsg("description saved! nya~");
                }
            } else {
                if(vis_count==0) break;
                Todo *t=&todos[vis[sel]];
                if(t->desc[0]) view_multiline("DESCRIPTION", t->desc);
                else if(popup_edit_ml("Task description",t->desc,sizeof t->desc)){
                    todos_save(); set_smsg("description saved! nya~");
                }
            }
            /* flush any queued input from editor/viewer interaction */
            timeout(0); while(getch()!=ERR); timeout(-1);
            break;
        }

        case ' ':
            if(view==VIEW_TASKS&&vis_count>0){
                Todo *t=&todos[vis[sel]];
                t->done=!t->done; t->done_at=t->done?time(NULL):0;
                todos_save(); rebuild_vis();
                bool all_done=true;
                for(int i=0;i<todo_count;i++) if(!todos[i].done){all_done=false;break;}
                if(all_done&&todo_count>0){celebrate=10;set_smsg("ALL DONE!! purrfect!! =^..^=");}
                else set_smsg(t->done?"done! good job :3":"back to pending~");
            }
            break;

        case 'p':
            if(view==VIEW_TASKS&&vis_count>0){
                Todo *t=&todos[vis[sel]]; t->pinned=!t->pinned;
                todos_save(); rebuild_vis();
                set_smsg(t->pinned?"pinned! *":"unpinned~");
            }
            break;

        case 't':{
            if(view!=VIEW_TASKS||vis_count==0) break;
            char hint[256]="enter tag (blank=clear)";
            if(utag_n>0){
                int hpos=(int)snprintf(hint,sizeof hint,"existing: ");
                for(int i=0;i<utag_n&&i<6&&hpos<(int)sizeof hint-2;i++)
                    hpos+=(int)snprintf(hint+hpos,(int)sizeof hint-hpos,"#%s ",utags[i]);
            }
            char buf[MAX_TAG]="";
            popup("Set tag",hint,buf,(int)sizeof buf);
            Todo *t=&todos[vis[sel]];
            if(buf[0]){
                char clean[MAX_TAG]=""; int ci=0;
                for(char *pp=buf;*pp&&ci<MAX_TAG-1;pp++)
                    if(!isspace((unsigned char)*pp)) clean[ci++]=(char)tolower((unsigned char)*pp);
                clean[ci]='\0';
                snprintf(t->tag, MAX_TAG, "%s", clean);
                set_smsg("tag set!");
            } else {snprintf(t->tag, MAX_TAG, "none");set_smsg("tag cleared");}
            todos_save(); rebuild_vis(); mark_dirty();
            /* flush any queued input from popup interaction */
            timeout(0); while(getch()!=ERR); timeout(-1);
            break;
        }

        case 'd':{
            if(view==VIEW_SCHED){
                if(sched_vis_count==0) break;
                Todo *t=&todos[sched_vis[sel_sched]];
                char buf[8]="",pr[80];
                snprintf(pr,sizeof pr,"delete scheduled \"%.40s\"? y/N",t->text);
                popup(pr,"",buf,(int)sizeof buf);
                if(buf[0]=='y'||buf[0]=='Y'){
                    int ti=sched_vis[sel_sched];
                    for(int i=ti;i<todo_count-1;i++) todos[i]=todos[i+1];
                    todo_count--;
                    todos_save(); rebuild_vis(); rebuild_sched_vis();
                    if(sel_sched>=sched_vis_count) sel_sched=sched_vis_count>0?sched_vis_count-1:0;
                    set_smsg("deleted! poof~");
                }
            } else if(view==VIEW_NOTES){
                if(note_vis_count==0) break;
                char buf[8]="";
                popup("delete this note? y/N","",buf,(int)sizeof buf);
                if(buf[0]=='y'||buf[0]=='Y'){
                    int ti=note_vis[sel_note];
                    for(int i=ti;i<note_count-1;i++) notes[i]=notes[i+1];
                    note_count--;
                    todos_save(); rebuild_note_vis();
                    if(sel_note>=note_vis_count) sel_note=note_vis_count>0?note_vis_count-1:0;
                    set_smsg("deleted! poof~");
                }
            } else {
                if(vis_count==0) break;
                char buf[8]="",pr[80];
                snprintf(pr,sizeof pr,"delete \"%.40s\"? y/N",todos[vis[sel]].text);
                popup(pr,"",buf,(int)sizeof buf);
                if(buf[0]=='y'||buf[0]=='Y'){
                    int ti=vis[sel];
                    for(int i=ti;i<todo_count-1;i++) todos[i]=todos[i+1];
                    todo_count--;
                    todos_save(); rebuild_vis();
                    /* adjust selection after visibility rebuild */
                    if(sel>=vis_count) sel=vis_count>0?vis_count-1:0;
                    set_smsg("deleted! poof~");
                }
            }
            /* flush any queued input from popup interaction */
            timeout(0); while(getch()!=ERR); timeout(-1);
            break;
        }

        case 'D':{
            if(view==VIEW_SCHED){
                if(sched_vis_count==0) break;
                char buf[8]="",pr[80];
                snprintf(pr,sizeof pr,"delete ALL %d scheduled tasks? y/N",sched_vis_count);
                popup(pr,"!! THIS CANNOT BE UNDONE !!",buf,(int)sizeof buf);
                if(buf[0]=='y'||buf[0]=='Y'){
                    int w=0;
                    for(int i=0;i<todo_count;i++)
                        if(!is_hidden(&todos[i])) todos[w++]=todos[i];
                    todo_count=w; sel_sched=0; sched_top=0;
                    todos_save(); rebuild_vis(); rebuild_sched_vis();
                    set_smsg("clean slate! fresh start nya~");
                }
            } else if(view==VIEW_NOTES){
                if(note_count==0) break;
                char buf[8]="",pr[80];
                snprintf(pr,sizeof pr,"delete ALL %d notes? y/N",note_count);
                popup(pr,"!! THIS CANNOT BE UNDONE !!",buf,(int)sizeof buf);
                if(buf[0]=='y'||buf[0]=='Y'){
                    note_count=0; sel_note=0; note_top=0;
                    todos_save(); rebuild_note_vis();
                    set_smsg("clean slate! fresh start nya~");
                }
            } else {
                if(todo_count==0) break;
                char buf[8]="",pr[80];
                snprintf(pr,sizeof pr,"delete ALL %d tasks? y/N",todo_count);
                popup(pr,"!! THIS CANNOT BE UNDONE !!",buf,(int)sizeof buf);
                if(buf[0]=='y'||buf[0]=='Y'){
                    todo_count=0; sel=0; list_top=0;
                    todos_save(); rebuild_vis(); mark_dirty();
                    set_smsg("clean slate! fresh start nya~");
                }
            }
            /* flush any queued input from popup interaction */
            timeout(0); while(getch()!=ERR); timeout(-1);
            break;
        }

        case 'S':{   /* schedule / reschedule */
            if(view==VIEW_SCHED){
                if(sched_vis_count==0) break;
                Todo *t=&todos[sched_vis[sel_sched]];
                char buf[MAX_LINE], hint[128];
                fmt_ymd(t->sched_at,buf,(int)sizeof buf);   /* prefill existing time */
                sched_hint(hint,(int)sizeof hint);
                SchedVal sv;
                if(popup_impl("Reschedule",hint,buf,(int)sizeof buf,validate_sched_time,&sv)){
                    t->sched_at=sv.when;
                    todos_save(); rebuild_vis(); rebuild_sched_vis();
                    char m[96],tsbuf[32];
                    fmt_ts(sv.when,tsbuf,(int)sizeof tsbuf);
                    snprintf(m,sizeof m,"will appear %s~",tsbuf);
                    set_smsg(m);
                }
            }
            else if(view==VIEW_TASKS){
                if(vis_count==0) break;
                Todo *t=&todos[vis[sel]];
                char buf[MAX_LINE]="", hint[128];
                sched_hint(hint,(int)sizeof hint);
                SchedVal sv;
                if(popup_impl("Schedule task",hint,buf,(int)sizeof buf,validate_sched_time,&sv)){
                    t->sched_at=sv.when;
                    todos_save();
                    rebuild_vis(); rebuild_sched_vis();
                    char m[96],tsbuf[32];
                    fmt_ts(sv.when,tsbuf,(int)sizeof tsbuf);
                    snprintf(m,sizeof m,"will appear %s~",tsbuf);
                    set_smsg(m);
                }
            }
            /* flush any queued input from popup interaction */
            timeout(0); while(getch()!=ERR); timeout(-1);
            break;
        }

        case 'u':{   /* release a scheduled task now */
            if(view==VIEW_SCHED && sched_vis_count>0){
                int ti=sched_vis[sel_sched];
                move_to_front(ti);
                todos[0].sched_at=0;
                todos_save();
                rebuild_vis(); rebuild_sched_vis();
                set_smsg("released to tasks! nya~");
            }
            break;
        }

        case '/':{
            char buf[MAX_LINE]="";
            if(popup("Search","type to filter~",buf,(int)sizeof buf))
                snprintf(search,sizeof search,"%s",buf);
            else search[0]='\0';
            sel=0; list_top=0; sel_note=0; note_top=0; sel_sched=0; sched_top=0;
            rebuild_vis(); rebuild_note_vis(); rebuild_sched_vis();
            /* flush any queued input from popup interaction */
            timeout(0); while(getch()!=ERR); timeout(-1);
            break;
        }

        case 27:
            search[0]='\0'; filter_tag[0]='\0';
            sel=0; list_top=0; sel_note=0; note_top=0; sel_sched=0; sched_top=0;
            rebuild_vis(); rebuild_note_vis(); rebuild_sched_vis(); set_smsg("filters cleared~");
            break;

        case '0':
            if(view==VIEW_TASKS){
                filter_tag[0]='\0'; sel=0; list_top=0;
                rebuild_vis(); set_smsg("showing all tasks");
            }
            break;

        case '1':case '2':case '3':case '4':case '5':case '6':{
            if(view==VIEW_TASKS){
                int ti=key-'1';
                if(ti<utag_n){
                    strncpy(filter_tag,utags[ti],MAX_TAG-1); filter_tag[MAX_TAG-1]='\0';
                    sel=0; list_top=0; rebuild_vis();
                    char m[64]; snprintf(m,sizeof m,"filter: #%s",filter_tag); set_smsg(m);
                }
            }
            break;
        }

        case KEY_RESIZE: break;
        default: break;
        }
    }

    free(todo_path);
    free(legacy_path);
    free_windows(&win);
    endwin();
    printf("bye bye~ =^..^=\n");
    return 0;
}

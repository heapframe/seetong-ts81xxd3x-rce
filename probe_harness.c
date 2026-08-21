/* Harness: dlopen libLOG.so, call TLog_Init, probe the RLog debug server.
   Output goes to /tmp/harness_out.txt (fd) because libLOG redirects stdout. */

typedef unsigned int size_t;
typedef int ssize_t;

extern void *memset(void *, int, size_t);
extern char *strncpy(char *, const char *, size_t);
extern size_t strlen(const char *);
extern unsigned int sleep(unsigned int);
extern int usleep(unsigned int);
extern ssize_t write(int, const void *, size_t);
extern ssize_t read(int, void *, size_t);
extern int close(int);
extern int socket(int, int, int);
extern int connect(int, const void *, unsigned int);
extern unsigned short htons(unsigned short);
extern int inet_pton(int, const char *, void *);
extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern int open(const char *, int, ...);
extern int sprintf(char *, const char *, ...);

#define AF_INET 2
#define SOCK_STREAM 1
#define RTLD_NOW 2
#define RTLD_GLOBAL 0x100
#define O_WRONLY 1
#define O_CREAT 0x40
#define O_TRUNC 0x200

struct sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    unsigned int sin_addr;
    unsigned char sin_zero[8];
};

static int outfd = -1;
static void out(const char *s){ if(outfd >= 0) write(outfd, s, strlen(s)); }

/* ---- stubs for symbols libLOG.so imports from edvr ---- */
int idvr_rnl_log(void){ return 0; }
int HY_PkgStream_SetLogdbg(void){ return 0; }
int _Z15MPU_BAK_GetSizePyiillyii(void){ return 0; }
int _Z16MPU_AVL_GetChNumv(void){ return 8; }
int _Z17MPU_AVL_GetStatusi(int x){ return 0; }
int _Z18MPU_AVL_IsChBandedi(int x){ return 0; }
int _Z19MPU_AVL_Pic_Capturei16enumSnapshotTypeiiPci(void){ return 0; }
int _Z19MPU_STR_Disk_GetNumPi(void){ return 0; }
int _Z20MPU_NET_GetP2pStatusv(void){ return 0; }
int _Z20MPU_PUB_GetDeviceCapP13tag_DeviceCap(void){ return 0; }
int _Z20MPU_STR_Disk_GetInfoiP11tagDiskInfoi(void){ return 0; }
int _Z23MPU_AVL_GetAudioSdpInfoiiPvj(void){ return 0; }
int _Z23MPU_AVL_GetStreamStatusiiPj(void){ return 0; }
int _Z23MPU_AVL_GetVideoSdpInfoiiPvj(void){ return 0; }
int _Z24MPU_AVL_GetLoginProtocoli(int x){ return 0; }
int _Z25MPU_STR_Disk_GetBusNumberjPi(void){ return 0; }
int _Z25MPU_STR_DiskGroup_GetInfoiP16tagDiskGroupInfo(void){ return 0; }
int _Z25MPU_USR_GetPasswordByNamePcS_i(char *n, char *outp, int l){
    if(outp && l>0){ strncpy(outp,"123456",l); }
    return 0;
}
int _Z26MPU_STR_Disk_GetDiskStatusjPiPm(void){ return 0; }
int _Z29MPU_AVL_GetRemainingBandwidthPj(void){ return 0; }
int _Z30MPU_AVL_GetChannelBitsOfOneMiniPjPi(void){ return 0; }

static void probe(int port, const char *payload){
    char line[1100];
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0){ out("socket fail\n"); return; }
    struct sockaddr_in a; memset(&a,0,sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if(connect(s,(struct sockaddr_in*)&a,sizeof a) < 0){
        sprintf(line, "connect port %d FAIL\n", port); out(line);
        close(s); return;
    }
    sprintf(line, "connect port %d OK\n", port); out(line);
    if(payload){
        write(s, payload, strlen(payload));
        char buf[900]; int n;
        int tries;
        for(tries=0; tries<6; tries++){
            extern int select(int, void *, void *, void *, void *);
            unsigned int tv[2]; tv[0]=1; tv[1]=0;
            unsigned char rfds[16]; memset(rfds,0,sizeof rfds);
            rfds[s>>5] |= (1u<<(s&31));
            int rv = select(s+1, rfds, 0, 0, tv);
            if(rv <= 0) continue;
            n = read(s, buf, sizeof(buf)-1);
            if(n > 0){
                buf[n]=0;
                sprintf(line, "REPLY[%d]: %s\n", n, buf);
                out(line);
                close(s); return;
            }
        }
        out("REPLY: timeout/none\n");
    }
    close(s);
}

int main(void);
void _start(void){
    int r = main();
    __asm__ volatile("mov r7, #1\n\tsvc #0\n" :: "r"(r));
    __builtin_unreachable();
}

int main(void){
    char line[256];
    outfd = open("/tmp/harness_out.txt", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    void *h = dlopen("/lib/libLOG.so", RTLD_NOW|RTLD_GLOBAL);
    if(!h){ out("dlopen failed\n"); return 1; }
    out("dlopen ok\n");
    int (*init)(void) = (int(*)(void))dlsym(h, "TLog_Init");
    if(init){
        int r = init();
        sprintf(line, "TLog_Init() -> %d\n", r); out(line);
    } else {
        out("no TLog_Init sym\n");
    }
    sleep(2);
    out("--- probe port 3000 ---\n");
    probe(3000, "Help\r\n");
    probe(3000, "Help\r");
    probe(3000, "GetSystemCfg\r\n");
    probe(3000, "GetSystemInfo 1 1 1\r\n");
    probe(3000, "Cmd id\r\n");
    out("--- done ---\n");
    close(outfd);
    return 0;
}

#include <stddef.h>

typedef unsigned int size_t;
typedef int ssize_t;

extern unsigned int sleep(unsigned int);
extern void *dlopen(const char *filename, int flags);
extern void *dlsym(void *handle, const char *symbol);

enum {
    RTLD_LAZY  = 0x0002,
    RTLD_GLOBAL = 0x0100,
};

/*
 * libLOG.so dependencies
 *
 * These symbols normally come from the firmware's edvr/application
 * environment. The harness provides minimal stubs so libLOG.so can
 * initialise under QEMU.
 */

int idvr_rnl_log(void)
{
    return 0;
}

int HY_PkgStream_SetLogdbg(void)
{
    return 0;
}

int _Z15MPU_BAK_GetSizePyiillyii(void)
{
    return 0;
}

int _Z16MPU_AVL_GetChNumv(void)
{
    return 8;
}

int _Z17MPU_AVL_GetStatusi(int channel)
{
    (void)channel;
    return 0;
}

int _Z18MPU_AVL_IsChBandedi(int channel)
{
    (void)channel;
    return 0;
}

int _Z19MPU_AVL_Pic_Capturei16enumSnapshotTypeiiPci(void)
{
    return 0;
}

int _Z19MPU_STR_Disk_GetNumPi(void)
{
    return 0;
}

int _Z20MPU_NET_GetP2pStatusv(void)
{
    return 0;
}

int _Z20MPU_PUB_GetDeviceCapP13tag_DeviceCap(void)
{
    return 0;
}

int _Z20MPU_STR_Disk_GetInfoiP11tagDiskInfoi(void)
{
    return 0;
}

int _Z23MPU_AVL_GetAudioSdpInfoiiPvj(void)
{
    return 0;
}

int _Z23MPU_AVL_GetStreamStatusiiPj(void)
{
    return 0;
}

int _Z23MPU_AVL_GetVideoSdpInfoiiPvj(void)
{
    return 0;
}

int _Z24MPU_AVL_GetLoginProtocoli(int protocol)
{
    (void)protocol;
    return 0;
}

int _Z25MPU_STR_Disk_GetBusNumberjPi(void)
{
    return 0;
}

int _Z25MPU_STR_DiskGroup_GetInfoiP16tagDiskGroupInfo(void)
{
    return 0;
}

int _Z25MPU_USR_GetPasswordByNamePcS_i(
    char *name,
    char *output,
    int output_len)
{
    (void)name;

    if (output != NULL && output_len > 0) {
        const char password[] = "123456";

        for (int i = 0; i < output_len - 1 && password[i] != '\0'; ++i)
            output[i] = password[i];

        output[output_len - 1] = '\0';
    }

    return 0;
}

int _Z26MPU_STR_Disk_GetDiskStatusjPiPm(void)
{
    return 0;
}

int _Z29MPU_AVL_GetRemainingBandwidthPj(void)
{
    return 0;
}

int _Z30MPU_AVL_GetChannelBitsOfOneMiniPjPi(void)
{
    return 0;
}


/*
 * Minimal process entry point.
 *
 * The binary is linked with -nostdlib, so we provide the entry point
 * and terminate directly through the ARM Linux syscall interface.
 */

int main(void);

void _start(void)
{
    const int status = main();

    __asm__ volatile(
        "mov r7, #1\n"
        "svc #0\n"
        :
        : "r"(status)
    );

    __builtin_unreachable();
}


/*
 * Load the firmware's real libLOG.so, initialise its logging subsystem,
 * and keep the process alive so the RLog server remains available.
 */

int main(void)
{
    void *handle = dlopen(
        "/lib/libLOG.so",
        RTLD_LAZY | RTLD_GLOBAL
    );

    if (handle == NULL)
        return 1;

    int (*TLog_Init)(void) =
        (int (*)(void))dlsym(handle, "TLog_Init");

    if (TLog_Init != NULL)
        TLog_Init();

    for (;;) {
        sleep(60);
    }

    return 0;
}
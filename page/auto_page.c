#include <config.h>
#include <render.h>
#include <stdlib.h>
#include <file.h>
#include <string.h>
#include <unistd.h>

static pthread_t g_tAutoPlayThreadID;
static pthread_mutex_t g_tAutoPlayThreadMutex = PTHREAD_MUTEX_INITIALIZER;
static int g_bAutoPlayThreadShouldExit = 0;
static T_PageCfg g_tPageCfg;

/* 以深度优先的方式获得目录下的文件
 * 即: 先获得顶层目录下的文件, 再进入一级子目录A
 *     先获得一级子目录A下的文件, 再进入二级子目录AA, ...
 *     处理完一级子目录A后, 再进入一级子目录B
 *
 * "连播模式"下调用该函数获得要显示的文件
 * 有两种方法获得这些文件:
 * 1. 事先只需要调用一次函数,把所有文件的名字保存到某个缓冲区中
 * 2. 要使用文件时再调用函数,只保存当前要使用的文件的名字
 * 第1种方法比较简单,但是当文件很多时有可能导致内存不足.
 * 使用第2种方法:
 * 假设某目录(包括所有子目录)下所有的文件都给它编一个号
 * g_iStartNumberToRecord : 从第几个文件开始取出它们的名字
 * g_iCurFileNumber       : 本次函数执行时读到的第1个文件的编号
 * g_iFileCountHaveGet    : 已经得到了多少个文件的名字
 * g_iFileCountTotal      : 每一次总共要取出多少个文件的名字
 * g_iNextProcessFileIndex: 在g_apstrFileNames数组中即将要显示在LCD上的文件
 *
 */
static int g_iStartNumberToRecord = 0;
static int g_iCurFileNumber = 0;
static int g_iFileCountHaveGet = 0;
static int g_iFileCountTotal = 0;
static int g_iNextProcessFileIndex = 0;

#define FILE_COUNT 10
static char g_apstrFileNames[FILE_COUNT][256];

static void ResetAutoPlayFile(void)
{
    g_iStartNumberToRecord = 0;
    g_iCurFileNumber = 0;
    g_iFileCountHaveGet = 0;
    g_iFileCountTotal = 0;
    g_iNextProcessFileIndex = 0;
}

static int GetNextAutoPlayFile(char *strFileName)
{
    int iError;

    if (g_iNextProcessFileIndex < g_iFileCountHaveGet)
    {
        strncpy(strFileName, g_apstrFileNames[g_iNextProcessFileIndex], 256);
        g_iNextProcessFileIndex++;
        return 0;
    }
    else
    {
        g_iCurFileNumber = 0;
        g_iFileCountHaveGet = 0;
        g_iFileCountTotal = FILE_COUNT;
        g_iNextProcessFileIndex = 0;
        iError = GetFilesIndir(g_tPageCfg.strSeletedDir, &g_iStartNumberToRecord, &g_iCurFileNumber, &g_iFileCountHaveGet, g_iFileCountTotal, g_apstrFileNames);
        if (iError || (g_iNextProcessFileIndex >= g_iFileCountHaveGet))
        {

            g_iStartNumberToRecord = 0;
            g_iCurFileNumber = 0;
            g_iFileCountHaveGet = 0;
            g_iFileCountTotal = FILE_COUNT;
            g_iNextProcessFileIndex = 0;

            iError = GetFilesIndir(g_tPageCfg.strSeletedDir, &g_iStartNumberToRecord, &g_iCurFileNumber, &g_iFileCountHaveGet, g_iFileCountTotal, g_apstrFileNames);
        }

        if (iError == 0)
        {
            if (g_iNextProcessFileIndex < g_iFileCountHaveGet)
            {
                strncpy(strFileName, g_apstrFileNames[g_iNextProcessFileIndex], 256);
                g_iNextProcessFileIndex++;
                return 0;
            }
        }
    }

    return -1;
}

static int IsImageFile(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    unsigned char header[4];
    if (file == NULL)
    {
        return 0;
    };
    if (fread(header, 1, 4, file) != 4)
    {
        fclose(file);
        return 0;
    };
    fclose(file);

    // 检查文件头部是否匹配常见的图片格式标识符
    if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF)
    {
        // JPEG 文件以 0xFF 0xD8 0xFF 开头
        return 1; // 是图片文件
    }
    else if (header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G')
    {
        // PNG 文件以 0x89 'P' 'N' 'G' 开头
        return 1; // 是图片文件
    }
    else if (header[0] == 'G' && header[1] == 'I' && header[2] == 'F' && header[3] == '8')
    {
        // GIF 文件以 'G' 'I' 'F' '8' 开头
        return 1; // 是图片文件
    }
    else if (header[0] == 0x42 && header[1] == 0x4D)
    {
        // BMP 文件以 0x42 0x4D 开头
        return 1; // 是图片文件
    }

    return 0; // 不是常见的图片文件格式
};

static PT_VideoMem PrepareNextPicture(int bCur)
{
    T_PixelDatas tOriginIconPixelDatas;
    T_PixelDatas tPicPixelDatas;
    PT_VideoMem ptVideoMem;
    int iError;
    int iXres, iYres, iBpp;
    int iTopLeftX, iTopLeftY;
    float k;
    char strFileName[256];
    GetDispResolution(&iXres, &iYres, &iBpp);

    ptVideoMem = GetVideoMem(-1, bCur);
    if (ptVideoMem == NULL)
    {
        DBG_PRINTF("can't get video mem for browse page!\n");
        return NULL;
    }
    ClearVideoMem(ptVideoMem, COLOR_BACKGROUND);

    while (1)
    {
        iError = GetNextAutoPlayFile(strFileName);
        if (!IsImageFile(strFileName))
        {
            continue;
        }
        if (IsImageFile(strFileName))
        {
            printf("这是一个图片文件。\n");
        }
        if (iError)
        {
            DBG_PRINTF("GetNextAutoPlayFile error\n");
            PutVideoMem(ptVideoMem);
            return NULL;
        }

        iError = GetPixelDatasFrmFile(strFileName, &tOriginIconPixelDatas);
        if (0 == iError)
        {
            break;
        }
    }

    k = (float)tOriginIconPixelDatas.iHeight / tOriginIconPixelDatas.iWidth;
    tPicPixelDatas.iWidth = iXres;
    tPicPixelDatas.iHeight = iXres * k;
    if (tPicPixelDatas.iHeight > iYres)
    {
        tPicPixelDatas.iWidth = iYres / k;
        tPicPixelDatas.iHeight = iYres;
    }
    tPicPixelDatas.iBpp = iBpp;
    tPicPixelDatas.iLineBytes = tPicPixelDatas.iWidth * tPicPixelDatas.iBpp / 8;
    tPicPixelDatas.iTotalBytes = tPicPixelDatas.iLineBytes * tPicPixelDatas.iHeight;
    tPicPixelDatas.aucPixelDatas = malloc(tPicPixelDatas.iTotalBytes);
    if (tPicPixelDatas.aucPixelDatas == NULL)
    {
        PutVideoMem(ptVideoMem);
        return NULL;
    }

    PicZoom(&tOriginIconPixelDatas, &tPicPixelDatas);

    iTopLeftX = (iXres - tPicPixelDatas.iWidth) / 2;
    iTopLeftY = (iYres - tPicPixelDatas.iHeight) / 2;
    PicMerge(iTopLeftX, iTopLeftY, &tPicPixelDatas, &ptVideoMem->tPixelDatas);
    FreePixelDatasFrmFile(&tOriginIconPixelDatas);
    free(tPicPixelDatas.aucPixelDatas);

    return ptVideoMem;
}

static void *AutoPlayThreadFunction(void *pVoid)
{
    int bExit;
    PT_VideoMem ptVideoMem;

    ResetAutoPlayFile();

    while (1)
    {

        pthread_mutex_lock(&g_tAutoPlayThreadMutex);
        bExit = g_bAutoPlayThreadShouldExit;
        pthread_mutex_unlock(&g_tAutoPlayThreadMutex);

        if (bExit)
        {
            return NULL;
        }

        ptVideoMem = PrepareNextPicture(0);

        if (ptVideoMem == NULL)
        {
            ptVideoMem = PrepareNextPicture(1);
        }

        FlushVideoMemToDev(ptVideoMem);

        PutVideoMem(ptVideoMem);

        sleep(g_tPageCfg.iIntervalSecond);
    }
    return NULL;
}

static void AutoPageRun(void)
{
    T_InputEvent tInputEvent;
    int iRet;

    g_bAutoPlayThreadShouldExit = 0;

    GetPageCfg(&g_tPageCfg);

    pthread_create(&g_tAutoPlayThreadID, NULL, AutoPlayThreadFunction, NULL);

    while (1)
    {
        iRet = GetInputEvent(&tInputEvent);
        if (0 == iRet)
        {
            pthread_mutex_lock(&g_tAutoPlayThreadMutex);
            g_bAutoPlayThreadShouldExit = 1;
            pthread_mutex_unlock(&g_tAutoPlayThreadMutex);
            sleep(2);
            return;
        }
    }
}

static T_PageAction g_tAutoPageAction = {
    .name = "auto",
    .Run = AutoPageRun,
};

int AutoPageInit(void)
{
    return RegisterPageAction(&g_tAutoPageAction);
}

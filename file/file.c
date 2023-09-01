
#include <config.h>
#include <file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int MapFile(PT_FileMap ptFileMap)
{
    int iFd;
    FILE *tFp;
    struct stat tStat;

    tFp = fopen(ptFileMap->strFileName, "r+");
    if (tFp == NULL)
    {
        DBG_PRINTF("can't open %s\n", ptFileMap->strFileName);
        return -1;
    }
    ptFileMap->tFp = tFp;
    iFd = fileno(tFp);

    fstat(iFd, &tStat);
    ptFileMap->iFileSize = tStat.st_size;
    ptFileMap->pucFileMapMem = (unsigned char *)mmap(NULL, tStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, iFd, 0);
    if (ptFileMap->pucFileMapMem == (unsigned char *)-1)
    {
        DBG_PRINTF("mmap error!\n");
        return -1;
    }
    return 0;
}

void UnMapFile(PT_FileMap ptFileMap)
{
    munmap(ptFileMap->pucFileMapMem, ptFileMap->iFileSize);
    fclose(ptFileMap->tFp);
}

static int isDir(char *strFilePath, char *strFileName)
{
    char strTmp[256];
    struct stat tStat;

    snprintf(strTmp, 256, "%s/%s", strFilePath, strFileName);
    strTmp[255] = '\0';

    if ((stat(strTmp, &tStat) == 0) && S_ISDIR(tStat.st_mode))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static int isRegDir(char *strDirPath, char *strSubDirName)
{
    static const char *astrSpecailDirs[] = {"sbin", "bin", "usr", "lib", "proc", "tmp", "dev", "sys", NULL};
    int i;

    if (0 == strcmp(strDirPath, "/"))
    {
        while (astrSpecailDirs[i])
        {
            if (0 == strcmp(strSubDirName, astrSpecailDirs[i]))
                return 0;
            i++;
        }
    }
    return 1;
}

static int isRegFile(char *strFilePath, char *strFileName)
{
    char strTmp[256];
    struct stat tStat;

    snprintf(strTmp, 256, "%s/%s", strFilePath, strFileName);
    strTmp[255] = '\0';

    if ((stat(strTmp, &tStat) == 0) && S_ISREG(tStat.st_mode))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int GetDirContents(char *strDirName, PT_DirContent **pptDirContents, int *piNumber)
{
    PT_DirContent *aptDirContents;
    struct dirent **aptNameList;
    int iNumber;
    int i;
    int j;

    iNumber = scandir(strDirName, &aptNameList, 0, alphasort);
    if (iNumber < 0)
    {
        DBG_PRINTF("scandir error : %s!\n", strDirName);
        return -1;
    }

    aptDirContents = malloc(sizeof(PT_DirContent) * (iNumber - 2));
    if (NULL == aptDirContents)
    {
        DBG_PRINTF("malloc error!\n");
        return -1;
    }
    *pptDirContents = aptDirContents;

    for (i = 0; i < iNumber - 2; i++)
    {
        aptDirContents[i] = malloc(sizeof(T_DirContent));
        if (NULL == aptDirContents)
        {
            DBG_PRINTF("malloc error!\n");
            return -1;
        }
    }

    for (i = 0, j = 0; i < iNumber; i++)
    {

        if ((0 == strcmp(aptNameList[i]->d_name, ".")) || (0 == strcmp(aptNameList[i]->d_name, "..")))
            continue;

        if (isDir(strDirName, aptNameList[i]->d_name))
        {
            strncpy(aptDirContents[j]->strName, aptNameList[i]->d_name, 256);
            aptDirContents[j]->strName[255] = '\0';
            aptDirContents[j]->eFileType = FILETYPE_DIR;
            free(aptNameList[i]);
            aptNameList[i] = NULL;
            j++;
        }
    }

    for (i = 0; i < iNumber; i++)
    {
        if (aptNameList[i] == NULL)
            continue;

        if ((0 == strcmp(aptNameList[i]->d_name, ".")) || (0 == strcmp(aptNameList[i]->d_name, "..")))
            continue;

        if (isRegFile(strDirName, aptNameList[i]->d_name))
        {
            strncpy(aptDirContents[j]->strName, aptNameList[i]->d_name, 256);
            aptDirContents[j]->strName[255] = '\0';
            aptDirContents[j]->eFileType = FILETYPE_FILE;
            free(aptNameList[i]);
            aptNameList[i] = NULL;
            j++;
        }
    }

    for (i = j; i < iNumber - 2; i++)
    {
        free(aptDirContents[i]);
    }

    for (i = 0; i < iNumber; i++)
    {
        if (aptNameList[i])
        {
            free(aptNameList[i]);
        }
    }
    free(aptNameList);

    *piNumber = j;

    return 0;
}

void FreeDirContents(PT_DirContent *aptDirContents, int iNumber)
{
    int i;
    for (i = 0; i < iNumber; i++)
    {
        free(aptDirContents[i]);
    }
    free(aptDirContents);
}

int GetFilesIndir(char *strDirName, int *piStartNumberToRecord, int *piCurFileNumber, int *piFileCountHaveGet, int iFileCountTotal, char apstrFileNames[][256])
{
    int iError;
    PT_DirContent *aptDirContents;
    int iDirContentsNumber;
    int i;
    char strSubDirName[256];

#define MAX_DIR_DEEPNESS 10
    static int iDirDeepness = 0;

    if (iDirDeepness > MAX_DIR_DEEPNESS)
    {
        return -1;
    }

    iDirDeepness++;

    iError = GetDirContents(strDirName, &aptDirContents, &iDirContentsNumber);
    if (iError)
    {
        DBG_PRINTF("GetDirContents error!\n");
        iDirDeepness--;
        return -1;
    }

    for (i = 0; i < iDirContentsNumber; i++)
    {
        if (aptDirContents[i]->eFileType == FILETYPE_FILE)
        {
            if (*piCurFileNumber >= *piStartNumberToRecord)
            {
                snprintf(apstrFileNames[*piFileCountHaveGet], 256, "%s/%s", strDirName, aptDirContents[i]->strName);
                (*piFileCountHaveGet)++;
                (*piCurFileNumber)++;
                (*piStartNumberToRecord)++;
                if (*piFileCountHaveGet >= iFileCountTotal)
                {
                    FreeDirContents(aptDirContents, iDirContentsNumber);
                    iDirDeepness--;
                    return 0;
                }
            }
            else
            {
                (*piCurFileNumber)++;
            }
        }
    }

    for (i = 0; i < iDirContentsNumber; i++)
    {
        if ((aptDirContents[i]->eFileType == FILETYPE_DIR) && isRegDir(strDirName, aptDirContents[i]->strName))
        {
            snprintf(strSubDirName, 256, "%s/%s", strDirName, aptDirContents[i]->strName);
            GetFilesIndir(strSubDirName, piStartNumberToRecord, piCurFileNumber, piFileCountHaveGet, iFileCountTotal, apstrFileNames);
            if (*piFileCountHaveGet >= iFileCountTotal)
            {
                FreeDirContents(aptDirContents, iDirContentsNumber);
                iDirDeepness--;
                return 0;
            }
        }
    }

    FreeDirContents(aptDirContents, iDirContentsNumber);
    iDirDeepness--;
    return 0;
}

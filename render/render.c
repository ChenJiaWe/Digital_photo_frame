#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <render.h>
#include <file.h>
#include <fonts_manager.h>
#include <encoding_manager.h>
#include <picfmt_manager.h>
#include <string.h>

void FlushVideoMemToDev(PT_VideoMem ptVideoMem)
{
	// memcpy(GetDefaultDispDev()->pucDispMem, ptVideoMem->tPixelDatas.aucPixelDatas, ptVideoMem.tPixelDatas.iHeight * ptVideoMem.tPixelDatas.iLineBytes);
	if (!ptVideoMem->bDevFrameBuffer)
	{
		GetDefaultDispDev()->ShowPage(ptVideoMem);
	}
}

int GetPixelDatasForIcon(char *strFileName, PT_PixelDatas ptPixelDatas)
{
	T_FileMap tFileMap;
	int iError;
	int iXres, iYres, iBpp;

	/* 图标存在 /etc/digitpic/icons */
	snprintf(tFileMap.strFileName, 128, "%s/%s", ICON_PATH, strFileName);
	tFileMap.strFileName[127] = '\0';

	iError = MapFile(&tFileMap);
	if (iError)
	{
		DBG_PRINTF("MapFile %s error!\n", strFileName);
		return -1;
	}

	iError = Parser("bmp")->isSupport(&tFileMap);
	if (iError == 0)
	{
		DBG_PRINTF("can't support this file: %s\n", strFileName);
		UnMapFile(&tFileMap);
		return -1;
	}

	GetDispResolution(&iXres, &iYres, &iBpp);
	ptPixelDatas->iBpp = iBpp;
	iError = Parser("bmp")->GetPixelDatas(&tFileMap, ptPixelDatas);
	if (iError)
	{
		DBG_PRINTF("GetPixelDatas for %s error!\n", strFileName);
		UnMapFile(&tFileMap);
		return -1;
	}

	UnMapFile(&tFileMap);
	return 0;
}

void FreePixelDatasForIcon(PT_PixelDatas ptPixelDatas)
{
	Parser("bmp")->FreePixelDatas(ptPixelDatas);
}

int GetPixelDatasFrmFile(char *strFileName, PT_PixelDatas ptPixelDatas)
{
	T_FileMap tFileMap;
	int iError;
	int iXres, iYres, iBpp;
	PT_PicFileParser ptParser;

	strncpy(tFileMap.strFileName, strFileName, 256);
	tFileMap.strFileName[255] = '\0';

	iError = MapFile(&tFileMap);
	if (iError)
	{
		DBG_PRINTF("MapFile %s error!\n", strFileName);
		return -1;
	}

	ptParser = GetParser(&tFileMap);
	if (ptParser == NULL)
	{
		UnMapFile(&tFileMap);
		return -1;
	}

	GetDispResolution(&iXres, &iYres, &iBpp);
	ptPixelDatas->iBpp = iBpp;
	iError = ptParser->GetPixelDatas(&tFileMap, ptPixelDatas);
	if (iError)
	{
		DBG_PRINTF("GetPixelDatas for %s error!\n", strFileName);
		UnMapFile(&tFileMap);
		return -1;
	}

	UnMapFile(&tFileMap);
	return 0;
}

void FreePixelDatasFrmFile(PT_PixelDatas ptPixelDatas)
{
	// Parser("bmp")->FreePixelDatas(ptPixelDatas);
	free(ptPixelDatas->aucPixelDatas);
}

/* 返回值: 设置了VideoMem中多少字节 */
static int SetColorForPixelInVideoMem(int iX, int iY, PT_VideoMem ptVideoMem, unsigned int dwColor)
{
	unsigned char *pucVideoMem;
	unsigned short *pwVideoMem16bpp;
	unsigned int *pdwVideoMem32bpp;
	unsigned short wColor16bpp; /* 565 */
	int iRed;
	int iGreen;
	int iBlue;

	pucVideoMem = ptVideoMem->tPixelDatas.aucPixelDatas;
	pucVideoMem += iY * ptVideoMem->tPixelDatas.iLineBytes + iX * ptVideoMem->tPixelDatas.iBpp / 8;
	pwVideoMem16bpp = (unsigned short *)pucVideoMem;
	pdwVideoMem32bpp = (unsigned int *)pucVideoMem;

	// DBG_PRINTF("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	// DBG_PRINTF("x = %d, y = %d\n", iX, iY);

	switch (ptVideoMem->tPixelDatas.iBpp)
	{
	case 8:
	{
		*pucVideoMem = (unsigned char)dwColor;
		return 1;
		break;
	}
	case 16:
	{
		iRed = (dwColor >> (16 + 3)) & 0x1f;
		iGreen = (dwColor >> (8 + 2)) & 0x3f;
		iBlue = (dwColor >> 3) & 0x1f;
		wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
		*pwVideoMem16bpp = wColor16bpp;
		return 2;
		break;
	}
	case 32:
	{
		*pdwVideoMem32bpp = dwColor;
		return 4;
		break;
	}
	default:
	{
		return -1;
	}
	}

	return -1;
}

void ClearRectangleInVideoMem(int iTopLeftX, int iTopLeftY, int iBotRightX, int iBotRightY, PT_VideoMem ptVideoMem, unsigned int dwColor)
{
	int x, y;
	for (y = iTopLeftY; y <= iBotRightY; y++)
		for (x = iTopLeftX; x <= iBotRightX; x++)
			SetColorForPixelInVideoMem(x, y, ptVideoMem, dwColor);
}

static int isFontInArea(int iTopLeftX, int iTopLeftY, int iBotRightX, int iBotRightY, PT_FontBitMap ptFontBitMap)
{
	if ((ptFontBitMap->iXLeft >= iTopLeftX) && (ptFontBitMap->iXMax <= iBotRightX) &&
			(ptFontBitMap->iYTop >= iTopLeftY) && (ptFontBitMap->iYMax <= iBotRightY))
		return 1;
	else
		return 0;
}

static int MergeOneFontToVideoMem(PT_FontBitMap ptFontBitMap, PT_VideoMem ptVideoMem)
{
	int i;
	int x, y;
	int bit;
	int iNum;
	unsigned char ucByte;

	if (ptFontBitMap->iBpp == 1)
	{
		for (y = ptFontBitMap->iYTop; y < ptFontBitMap->iYMax; y++)
		{
			i = (y - ptFontBitMap->iYTop) * ptFontBitMap->iPitch;
			for (x = ptFontBitMap->iXLeft, bit = 7; x < ptFontBitMap->iXMax; x++)
			{
				if (bit == 7)
				{
					ucByte = ptFontBitMap->pucBuffer[i++];
				}

				if (ucByte & (1 << bit))
				{
					iNum = SetColorForPixelInVideoMem(x, y, ptVideoMem, COLOR_FOREGROUND);
				}
				else
				{
					/* 使用背景色 */
					// g_ptDispOpr->ShowPixel(x, y, 0); /* 黑 */
					iNum = SetColorForPixelInVideoMem(x, y, ptVideoMem, COLOR_BACKGROUND);
				}
				if (iNum == -1)
				{
					return -1;
				}
				bit--;
				if (bit == -1)
				{
					bit = 7;
				}
			}
		}
	}
	else if (ptFontBitMap->iBpp == 8)
	{
		for (y = ptFontBitMap->iYTop; y < ptFontBitMap->iYMax; y++)
			for (x = ptFontBitMap->iXLeft; x < ptFontBitMap->iXMax; x++)
			{
				// g_ptDispOpr->ShowPixel(x, y, ptFontBitMap->pucBuffer[i++]);
				if (ptFontBitMap->pucBuffer[i++])
				{
					iNum = SetColorForPixelInVideoMem(x, y, ptVideoMem, COLOR_FOREGROUND);
				}
				else
				{
					iNum = SetColorForPixelInVideoMem(x, y, ptVideoMem, COLOR_BACKGROUND);
				}

				if (iNum == -1)
				{
					return -1;
				}
			}
	}
	else
	{
		DBG_PRINTF("ShowOneFont error, can't support %d bpp\n", ptFontBitMap->iBpp);
		return -1;
	}
	return 0;
}

int MergerStringToCenterOfRectangleInVideoMem(int iTopLeftX, int iTopLeftY, int iBotRightX, int iBotRightY, unsigned char *pucTextString, PT_VideoMem ptVideoMem)
{
	int iLen;
	int iError;
	unsigned char *pucBufStart;
	unsigned char *pucBufEnd;
	unsigned int dwCode;
	T_FontBitMap tFontBitMap;

	int bHasGetCode = 0;

	int iMinX = 32000, iMaxX = -1;
	int iMinY = 32000, iMaxY = -1;

	int iStrTopLeftX, iStrTopLeftY;

	int iWidth, iHeight;

	tFontBitMap.iCurOriginX = 0;
	tFontBitMap.iCurOriginY = 0;
	pucBufStart = pucTextString;
	pucBufEnd = pucTextString + strlen((char *)pucTextString);

	ClearRectangleInVideoMem(iTopLeftX, iTopLeftY, iBotRightX, iBotRightY, ptVideoMem, COLOR_BACKGROUND);

	while (1)
	{

		iLen = GetCodeFrmBuf(pucBufStart, pucBufEnd, &dwCode);
		if (0 == iLen)
		{

			if (!bHasGetCode)
			{
				// DBG_PRINTF("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
				return -1;
			}
			else
			{
				break;
			}
		}

		bHasGetCode = 1;
		pucBufStart += iLen;

		iError = GetFontBitmap(dwCode, &tFontBitMap);
		if (0 == iError)
		{
			if (iMinX > tFontBitMap.iXLeft)
			{
				iMinX = tFontBitMap.iXLeft;
			}
			if (iMaxX < tFontBitMap.iXMax)
			{
				iMaxX = tFontBitMap.iXMax;
			}

			if (iMinY > tFontBitMap.iYTop)
			{
				iMinY = tFontBitMap.iYTop;
			}
			if (iMaxY < tFontBitMap.iXMax)
			{
				iMaxY = tFontBitMap.iYMax;
			}

			tFontBitMap.iCurOriginX = tFontBitMap.iNextOriginX;
			tFontBitMap.iCurOriginY = tFontBitMap.iNextOriginY;
		}
		else
		{
			DBG_PRINTF("GetFontBitmap for calc width/height error!\n");
		}
	}

	iWidth = iMaxX - iMinX;
	iHeight = iMaxY - iMinY;

	if (iWidth > iBotRightX - iTopLeftX)
	{
		iWidth = iBotRightX - iTopLeftX;
	}

	if (iHeight > iBotRightY - iTopLeftY)
	{
		DBG_PRINTF("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
		// DBG_PRINTF("iHeight = %d, iBotRightY - iTopLeftX = %d - %d = %d\n", iHeight, iBotRightY, iTopLeftY, iBotRightY - iTopLeftY);
		return -1;
	}
	// DBG_PRINTF("iWidth = %d, iHeight = %d\n", iWidth, iHeight);

	iStrTopLeftX = iTopLeftX + (iBotRightX - iTopLeftX - iWidth) / 2;
	iStrTopLeftY = iTopLeftY + (iBotRightY - iTopLeftY - iHeight) / 2;
	// DBG_PRINTF("iNewFirstFontTopLeftX = %d, iNewFirstFontTopLeftY = %d\n", iNewFirstFontTopLeftX, iNewFirstFontTopLeftY);

	tFontBitMap.iCurOriginX = iStrTopLeftX - iMinX;
	tFontBitMap.iCurOriginY = iStrTopLeftY - iMinY;

	// DBG_PRINTF("iCurOriginX = %d, iCurOriginY = %d\n", tFontBitMap.iCurOriginX, tFontBitMap.iCurOriginY);

	pucBufStart = pucTextString;
	bHasGetCode = 0;
	while (1)
	{

		iLen = GetCodeFrmBuf(pucBufStart, pucBufEnd, &dwCode);
		if (0 == iLen)
		{

			if (!bHasGetCode)
			{
				DBG_PRINTF("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
				return -1;
			}
			else
			{
				break;
			}
		}

		bHasGetCode = 1;
		pucBufStart += iLen;

		iError = GetFontBitmap(dwCode, &tFontBitMap);
		if (0 == iError)
		{
			// DBG_PRINTF("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

			if (isFontInArea(iTopLeftX, iTopLeftY, iBotRightX, iBotRightY, &tFontBitMap))
			{
				if (MergeOneFontToVideoMem(&tFontBitMap, ptVideoMem))
				{
					DBG_PRINTF("MergeOneFontToVideoMem error for code 0x%x\n", dwCode);
					return -1;
				}
			}
			else
			{
				return 0;
			}
			// DBG_PRINTF("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

			tFontBitMap.iCurOriginX = tFontBitMap.iNextOriginX;
			tFontBitMap.iCurOriginY = tFontBitMap.iNextOriginY;
		}
		else
		{
			DBG_PRINTF("GetFontBitmap for drawing error!\n");
		}
	}

	return 0;
}

static void InvertButton(PT_Layout ptLayout)
{
	int iY;
	int i;
	int iButtonWidthBytes;
	unsigned char *pucVideoMem;
	PT_DispOpr ptDispOpr = GetDefaultDispDev();

	pucVideoMem = ptDispOpr->pucDispMem;
	pucVideoMem += ptLayout->iTopLeftY * ptDispOpr->iLineWidth + ptLayout->iTopLeftX * ptDispOpr->iBpp / 8; /* ͼ����Framebuffer�еĵ�ַ */
	iButtonWidthBytes = (ptLayout->iBotRightX - ptLayout->iTopLeftX + 1) * ptDispOpr->iBpp / 8;

	for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
	{
		for (i = 0; i < iButtonWidthBytes; i++)
		{
			pucVideoMem[i] = ~pucVideoMem[i]; /* ȡ�� */
		}
		pucVideoMem += ptDispOpr->iLineWidth;
	}
}

void ReleaseButton(PT_Layout ptLayout)
{
	InvertButton(ptLayout);
}

void PressButton(PT_Layout ptLayout)
{
	InvertButton(ptLayout);
}

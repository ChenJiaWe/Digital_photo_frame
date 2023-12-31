#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <encoding_manager.h>
#include <fonts_manager.h>
#include <disp_manager.h>
#include <input_manager.h>
#include <pic_operation.h>
#include <render.h>
#include <string.h>
#include <picfmt_manager.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

/* digitpic <freetype_file> */
int main(int argc, char **argv)
{
	int iError;

	DebugInit();
	InitDebugChanel();

	DisplayInit();
	SelectAndInitDefaultDispDev("fb");

	AllocVideoMem(5);

	InputInit();
	AllInputDevicesInit();

	iError = FontsInit();
	if (iError)
	{
		DBG_PRINTF("FontsInit error!\n");
	}

	iError = SetFontsDetail("freetype", "./SIMSUN.TTC", 24);
	if (iError)
	{
		DBG_PRINTF("SetFontsDetail error!\n");
	}

	PagesInit();

	PicFmtsInit();

	Page("main")->Run();

	return 0;
}

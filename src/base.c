
#include "functions.h"

//static CMUTIL_Mutex *g_cmutil_init_mutex = NULL;
static int g_cmutil_init_cnt = 0;

void CMUTIL_Init(CMUTIL_MemOper memoper)
{
	if (g_cmutil_init_cnt == 0) {
		CMUTIL_CallStackInit();
		CMUTIL_MemDebugInit(memoper);
		CMUTIL_ThreadInit();
		CMUTIL_XmlInit();
		CMUTIL_NetworkInit();
		CMUTIL_LogInit();
	}
	g_cmutil_init_cnt++;
}

void CMUTIL_Clear()
{
	if (g_cmutil_init_cnt > 0) {
		g_cmutil_init_cnt--;
		if (g_cmutil_init_cnt == 0) {
			CMUTIL_LogClear();
			CMUTIL_NetworkClear();
			CMUTIL_XmlClear();
			CMUTIL_ThreadClear();
			CMUTIL_MemDebugClear();
			CMUTIL_CallStackClear();
		}
	}
}

void CMUTIL_UnusedP(void *p,...)
{
	(void)(p);
	// do nothing.
}

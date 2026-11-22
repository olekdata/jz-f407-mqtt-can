"""Run host-side tests of RTC/NTP and daily log routing, without accessing hardware."""

import argparse
import datetime as dt
import os
import pathlib
import subprocess
import tempfile

project = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--vcvars", type=pathlib.Path, help="Path to MSVC vcvars64.bat")
args = parser.parse_args()
if os.name != "nt":
    parser.error("This host-side test requires Windows and the MSVC C compiler")
vcvars = args.vcvars
if vcvars is None:
    vswhere = pathlib.Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere /= r"Microsoft Visual Studio\Installer\vswhere.exe"
    if not vswhere.is_file():
        parser.error("Cannot find vswhere.exe; provide --vcvars with vcvars64.bat")
    installation = subprocess.check_output(
        [str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
         "-property", "installationPath"],
        text=True,
    ).strip()
    if not installation:
        parser.error("MSVC C/C++ tools not found; provide --vcvars with vcvars64.bat")
    vcvars = pathlib.Path(installation) / r"VC\Auxiliary\Build\vcvars64.bat"
if not vcvars.is_file():
    parser.error(f"Cannot find compiler environment: {vcvars}")


def function(source, signature):
    start = source.index(signature + "\n{")
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


support = r"""
#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
typedef struct { int locked; } StaticSemaphore_t;
typedef StaticSemaphore_t *osMutexId_t;
typedef void *osSemaphoreId_t;
typedef struct {
 const char *name; uint32_t attr_bits; void *cb_mem; uint32_t cb_size;
} osMutexAttr_t;
#define osMutexPrioInherit 1
#define osWaitForever UINT32_MAX
#define osOK 0
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_ERROR 1
#define RTC_BKP_DR0 0
#define RTC_FORMAT_BIN 0
#define RTC_FORMAT_BCD 1
#define RTC_DAYLIGHTSAVING_NONE 0
#define RTC_STOREOPERATION_RESET 0
#define RTC_HOURFORMAT_24 0
#define RTC_OUTPUT_DISABLE 0
#define RTC_OUTPUT_POLARITY_HIGH 0
#define RTC_OUTPUT_TYPE_OPENDRAIN 0
#define RTC_WEEKDAY_MONDAY 1
#define RTC_MONTH_JANUARY 1
#define RTC 1
typedef struct {
 uint8_t Hours, Minutes, Seconds; uint32_t DayLightSaving, StoreOperation;
} RTC_TimeTypeDef;
typedef struct { uint8_t WeekDay, Month, Date, Year; } RTC_DateTypeDef;
typedef struct {
 int Instance;
 struct { unsigned HourFormat, AsynchPrediv, SynchPrediv, OutPut,
 OutPutPolarity, OutPutType; } Init;
} RTC_HandleTypeDef;
static RTC_HandleTypeDef hrtc;
static RTC_TimeTypeDef fake_time;
static RTC_DateTypeDef fake_date;
static uint32_t backup, ticks;
static bool calendar_initialized = true, fail_set_date, fail_get_time;
static unsigned set_time_calls, set_date_calls;
static uint32_t HAL_GetTick(void) { return ticks; }
static unsigned osKernelGetTickCount(void) { return ticks; }
static unsigned osKernelGetTickFreq(void) { return 1000; }
static void Error_Handler(void) { assert(!"Error_Handler"); abort(); }
static osMutexId_t osMutexNew(const osMutexAttr_t *a) { return a->cb_mem; }
static int osMutexAcquire(osMutexId_t m, uint32_t t) {
 (void)t; assert(m && !m->locked); m->locked=1; return osOK;
}
static int osMutexRelease(osMutexId_t m) {
 assert(m && m->locked); m->locked=0; return osOK;
}
static HAL_StatusTypeDef HAL_RTC_Init(RTC_HandleTypeDef *h) {
 (void)h; return HAL_OK;
}
#define __HAL_RTC_IS_CALENDAR_INITIALIZED(h) (calendar_initialized)
static uint32_t HAL_RTCEx_BKUPRead(RTC_HandleTypeDef *h, int r) {
 (void)h; (void)r; return backup;
}
static void HAL_RTCEx_BKUPWrite(RTC_HandleTypeDef *h, int r, uint32_t v) {
 (void)h; (void)r; backup=v;
}
static HAL_StatusTypeDef HAL_RTC_GetTime(RTC_HandleTypeDef *h,
 RTC_TimeTypeDef *t, int f) {
 (void)h; (void)f; *t=fake_time; return fail_get_time ? HAL_ERROR : HAL_OK;
}
static HAL_StatusTypeDef HAL_RTC_GetDate(RTC_HandleTypeDef *h,
 RTC_DateTypeDef *d, int f) {
 (void)h; (void)f; *d=fake_date; return HAL_OK;
}
static HAL_StatusTypeDef HAL_RTC_SetTime(RTC_HandleTypeDef *h,
 RTC_TimeTypeDef *t, int f) {
 (void)h; (void)f; fake_time=*t; ++set_time_calls; return HAL_OK;
}
static HAL_StatusTypeDef HAL_RTC_SetDate(RTC_HandleTypeDef *h,
 RTC_DateTypeDef *d, int f) {
 (void)h; (void)f; ++set_date_calls;
 if (fail_set_date) return HAL_ERROR;
 fake_date=*d; return HAL_OK;
}
typedef struct { uint32_t addr; } ip_addr_t;
#define IP_ADDR4(p,a,b,c,d) ((p)->addr=((a)<<24)|((b)<<16)|((c)<<8)|(d))
#define SNTP_OPMODE_POLL 0
static bool sntp_active;
static int sntp_enabled(void) { return sntp_active; }
static void sntp_setserver(int n, const ip_addr_t *a) { (void)n; (void)a; }
static void sntp_setoperatingmode(int m) { (void)m; }
static void sntp_init(void) { sntp_active=true; }
typedef struct {
 uint8_t *storage; size_t item_size;
 unsigned depth, count, head, tail;
} StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
static bool escape_on_empty;
static jmp_buf sd_exit;
static QueueHandle_t xQueueCreateStatic(unsigned depth, size_t item_size,
 uint8_t *storage, StaticQueue_t *q) {
 *q=(StaticQueue_t){storage,item_size,depth,0,0,0}; return q;
}
static int xQueueSend(QueueHandle_t q, const void *item, unsigned wait) {
 (void)wait; if(q->count==q->depth) return 0;
 memcpy(q->storage+q->tail*q->item_size,item,q->item_size);
 q->tail=(q->tail+1)%q->depth; ++q->count; return pdPASS;
}
static int xQueueReceive(QueueHandle_t q, void *item, unsigned wait) {
 (void)wait;
 if(!q->count) { if(escape_on_empty) longjmp(sd_exit,1); return 0; }
 memcpy(item,q->storage+q->head*q->item_size,q->item_size);
 q->head=(q->head+1)%q->depth; --q->count; return pdPASS;
}
static struct tm *host_gmtime_r(const time_t *t, struct tm *out) {
 return gmtime_s(out,t)==0 ? out : NULL;
}
#define gmtime_r host_gmtime_r
#endif
"""

sd_support = r"""
typedef unsigned DWORD;
typedef unsigned UINT;
typedef unsigned char BYTE;
typedef int FRESULT;
typedef int FATFS;
typedef struct { int index; } FIL;
#define FR_OK 0
#define FR_DISK_ERR 1
#define FA_WRITE 2
#define FA_OPEN_APPEND 0x30
#define SD_PRESENT 1
static uint8_t retSD;
static FATFS SDFatFS;
static char SDPath[]="0:";
static struct { char path[40], content[4096]; unsigned closes; } files[8];
static unsigned file_count;
static int BSP_PlatformIsDetected(void) { return SD_PRESENT; }
static void osThreadExit(void) { assert(!"unexpected thread exit"); }
static void osDelay(unsigned t) { ticks+=t; assert(!"unexpected retry"); }
static FRESULT f_mount(FATFS *f, const char *p, unsigned m) {
 (void)f; (void)p; (void)m; return FR_OK;
}
static FRESULT f_getfree(const char *p, DWORD *free_clusters, FATFS **f) {
 (void)p; (void)f; *free_clusters=100; return FR_OK;
}
static FRESULT f_open(FIL *f, const char *path, BYTE mode) {
 assert(mode==(FA_WRITE|FA_OPEN_APPEND));
 for(unsigned i=0;i<file_count;i++) {
  if(!strcmp(files[i].path,path)) { f->index=i; return FR_OK; }
 }
 assert(file_count<8); f->index=file_count++;
 strcpy(files[f->index].path,path); return FR_OK;
}
static FRESULT f_close(FIL *f) { ++files[f->index].closes; return FR_OK; }
static FRESULT f_sync(FIL *f) { (void)f; return FR_OK; }
static FRESULT f_write(FIL *f, const void *data, UINT size, UINT *written) {
 size_t used=strlen(files[f->index].content);
 assert(used+size<sizeof(files[f->index].content));
 memcpy(files[f->index].content+used,data,size);
 files[f->index].content[used+size]=0; *written=size; return FR_OK;
}
static void RunSDCardTest(void) {}
"""

cases = [
    ("2026-03-29T00:59:59", "2026-03-29 01:59:59", 0),
    ("2026-03-29T01:00:00", "2026-03-29 03:00:00", 1),
    ("2026-10-25T00:59:59", "2026-10-25 02:59:59", 1),
    ("2026-10-25T01:00:00", "2026-10-25 02:00:00", 0),
    ("2026-12-31T23:00:00", "2027-01-01 00:00:00", 0),
    ("2028-02-28T23:00:00", "2028-02-29 00:00:00", 0),
    ("2040-06-30T22:00:00", "2040-07-01 00:00:00", 1),
]
checks = []
for utc, expected, summer in cases:
    seconds = int(dt.datetime.fromisoformat(utc).replace(tzinfo=dt.timezone.utc).timestamp())
    checks.append(
        f'my_time_ntp_received({seconds}U); my_time_poll();'
        f'assert(my_time_get_local(&local));'
        f'strftime(text,sizeof(text),"%Y-%m-%d %H:%M:%S",&local);'
        f'assert(!strcmp(text,"{expected}")); assert(local.tm_isdst=={summer});'
    )

tests = r"""
int main(void) {
 struct tm local; char text[32]; LogSDEntry entry;
 assert(my_time_init()); assert(log_sd_queue_init());
 assert(!my_time_get_local(&local));
 log_put("before NTP");
 assert(log_sd_queue_get(&entry,0)); assert(entry.date==0);
 assert(strstr(entry.line,"0:00:00-before NTP\n"));
 CHECKS
 /* A retained, synchronized calendar must not be overwritten on boot. */
 unsigned old_set_time=set_time_calls, old_set_date=set_date_calls;
 MX_RTC_Init();
 assert(set_time_calls==old_set_time && set_date_calls==old_set_date);
 /* Reject bogus NTP, and never mark a partially written calendar valid. */
 my_time_ntp_received(0); my_time_poll();
 assert(set_time_calls==old_set_time);
 fail_set_date=true;
 my_time_ntp_received(1794783600U); my_time_poll();
 assert(backup==0 && !my_time_get_local(&local));
 fail_set_date=false;
 my_time_ntp_received(1794783600U); my_time_poll();
 assert(backup==MY_TIME_RTC_MARKER);
 fail_get_time=true; assert(!my_time_get_local(&local)); fail_get_time=false;
 assert(log_sd_queue_init());
 log_put("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
 assert(log_sd_queue_get(&entry,0));
 assert(entry.date==20261116U);
 assert(strlen(entry.line)==LOG_LEN-1 && entry.line[LOG_LEN-2]=='\n');
 /* Route queued dates, including backward corrections, by event date. */
 assert(log_sd_queue_init());
 const uint32_t dates[]={0,20261231,20270101,20261231};
 const char *lines[]={"unsynced\n","old-day\n","new-day\n","corrected-back\n"};
 for(unsigned i=0;i<4;i++) {
  entry.date=dates[i]; strcpy(entry.line,lines[i]);
  assert(xQueueSend(log_sd_queue_handle,&entry,0)==pdPASS);
 }
 escape_on_empty=true;
 if(setjmp(sd_exit)==0) StartSDTask(NULL);
 escape_on_empty=false;
 assert(file_count==4); /* Last file also includes current-dated mount diagnostics. */
 assert(!strcmp(files[0].path,"0:log_unsynced.txt"));
 assert(!strcmp(files[1].path,"0:log20261231.txt"));
 assert(!strcmp(files[2].path,"0:log20270101.txt"));
 assert(strstr(files[1].content,"old-day\ncorrected-back\n"));
 assert(strstr(files[2].content,"new-day\n"));
 assert(files[0].closes==1 && files[1].closes==2 && files[2].closes==1);
 puts("PASS: RTC/NTP validation, retained calendar, CET/CEST transitions, leap/year rollover, long log lines and queued daily append routing");
 return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="jz-rtc-test-") as directory:
    root = pathlib.Path(directory)
    for header in ("rtc.h", "FreeRTOS.h", "task.h", "semphr.h", "cmsis_os2.h",
                   "queue.h", "stats.h", "lwip/apps/sntp.h"):
        path = root / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "support.h"\n')
    (root / "support.h").write_text(support)
    freertos = (project / "Core" / "Src" / "freertos.c").read_text(encoding="utf-8")
    rtc = (project / "Core" / "Src" / "rtc.c").read_text(encoding="utf-8")
    generated = '#include "support.h"\n'
    generated += f'#include "{project / "Core" / "Src" / "my_time.c"}"\n'
    generated += 'static StaticSemaphore_t log_mutex_storage;\n'
    generated += 'osMutexId_t logMutexHandle=&log_mutex_storage;\n'
    generated += f'#include "{project / "Core" / "Src" / "my_log.c"}"\n'
    generated += function(rtc, "void MX_RTC_Init(void)") + "\n"
    generated += sd_support
    for signature in ("static void LogFatFsError(const char *operation, FRESULT result)",
                      "static FRESULT AppendSDLogLine(FIL *file, const char *line)",
                      "static void StartSDTask(void *argument)"):
        generated += function(freertos, signature) + "\n"
    generated += tests.replace("CHECKS", "\n".join(checks))
    (root / "test.c").write_text(generated)
    command = (
        f'call "{vcvars}" >nul && cd /d "{root}" && '
        f'cl /nologo /std:c11 /W3 /D_CRT_SECURE_NO_WARNINGS '
        f'/I"{root}" /I"{project / "Core" / "Inc"}" test.c /Fe:test.exe && test.exe'
    )
    subprocess.run(command, shell=True, check=True)

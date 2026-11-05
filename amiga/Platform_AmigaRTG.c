/* ClassiCube's platform layer for AmigaOS 3.x (68040/68060 + FPU), with
 * libnix (-noixemul). Files go through dos.library; paths stay as the game
 * writes them ("maps/x.cw"), which AmigaDOS reads relative to the current
 * drawer, and the current drawer is PROGDIR: while the game runs. One task:
 * no threads (the map generator runs in steps, CC_BUILD_COOPTHREADED), no
 * sockets, no plugins, no updater. Started without arguments (from the
 * Workbench or a bare Shell command) it goes straight into singleplayer.
 * BSD-3-Clause, like ClassiCube. */
#include "../src/Core.h"
#if defined CC_BUILD_AMIGA_RTG

#define CC_NO_UPDATER
#define CC_NO_DYNLIB
#define CC_NO_SOCKETS
#define CC_NO_THREADING
#define CC_NO_ENCRYPTION
#define CC_NO_OPEN
#define CC_NO_CRASHHANDLER

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "../src/_PlatformBase.h"
#include "../src/Stream.h"
#include "../src/ExtMath.h"
#include "../src/SystemFonts.h"
#include "../src/Funcs.h"
#include "../src/Window.h"
#include "../src/Utils.h"
#include "../src/Errors.h"


/* AmigaDOS error codes (dos/dos.h) */
const cc_result ReturnCode_FileShareViolation = ERROR_OBJECT_IN_USE;    /* 202 */
const cc_result ReturnCode_FileNotFound       = ERROR_OBJECT_NOT_FOUND; /* 205 */
const cc_result ReturnCode_PathNotFound       = ERROR_DIR_NOT_FOUND;    /* 204 */
const cc_result ReturnCode_DirectoryExists    = ERROR_OBJECT_EXISTS;    /* 203 */

const char* Platform_AppNameSuffix = " Amiga";
cc_uint8 Platform_Flags = PLAT_FLAG_SINGLE_PROCESS;
cc_bool  Platform_ReadonlyFilesystem;

void amiga_screens_snapshot(void);
void amiga_screens_close_leftovers(void);

static BPTR amiga_oldDir;
static cc_bool amiga_dirChanged;

static void Amiga_Cleanup(void) {
	static cc_bool done;
	if (done) return;
	done = true;

	SDL_Quit();
	amiga_screens_close_leftovers();

	if (amiga_dirChanged) {
		BPTR ours = CurrentDir(amiga_oldDir);
		UnLock(ours);
		amiga_dirChanged = false;
	}
}


/*########################################################################################################################*
*-----------------------------------------------------Main entrypoint-----------------------------------------------------*
*#########################################################################################################################*/
#include "../src/main_impl.h"

int main(int argc, char** argv) {
	cc_result res;
	amiga_screens_snapshot();
	atexit(Amiga_Cleanup);

	SetupProgram(argc, argv);
	res = RunProgram(argc, argv);

	Window_Free();
	Process_Exit(res);
	return res;
}


/*########################################################################################################################*
*------------------------------------------------------Logging/Time-------------------------------------------------------*
*#########################################################################################################################*/
void Platform_Log(const char* msg, int len) {
	BPTR out = Output();
	if (!out) return; /* started from the Workbench */
	Write(out, (APTR)msg, len);
	Write(out, (APTR)"\n", 1);
}

#define UNIX_EPOCH_SECONDS 62135596800ULL

TimeMS DateTime_CurrentUTC(void) {
	struct timeval cur;
	gettimeofday(&cur, NULL);
	return (cc_uint64)cur.tv_sec + UNIX_EPOCH_SECONDS;
}

void DateTime_CurrentLocal(struct cc_datetime* t) {
	time_t s = time(NULL);
	struct tm* loc = localtime(&s);

	if (!loc) { Mem_Set(t, 0, sizeof(*t)); return; }
	t->year   = loc->tm_year + 1900;
	t->month  = loc->tm_mon  + 1;
	t->day    = loc->tm_mday;
	t->hour   = loc->tm_hour;
	t->minute = loc->tm_min;
	t->second = loc->tm_sec;
}


/*########################################################################################################################*
*--------------------------------------------------------Stopwatch--------------------------------------------------------*
*#########################################################################################################################*/
#define US_PER_SEC 1000000ULL

cc_uint64 Stopwatch_Measure(void) {
	struct timeval cur;
	gettimeofday(&cur, NULL);
	return (cc_uint64)cur.tv_sec * US_PER_SEC + cur.tv_usec;
}

cc_uint64 Stopwatch_ElapsedMicroseconds(cc_uint64 beg, cc_uint64 end) {
	if (end < beg) return 0;
	return end - beg;
}


/*########################################################################################################################*
*-----------------------------------------------------Directory/File------------------------------------------------------*
*#########################################################################################################################*/
/* "a/b" means the same to AmigaDOS as elsewhere (relative to the current */
/*  drawer); only a leading "./" (which AmigaDOS would not know) is dropped. */
void Platform_EncodePath(cc_filepath* dst, const cc_string* path) {
	cc_string str = *path;
	int len;

	while (str.length >= 2 && str.buffer[0] == '.' && str.buffer[1] == '/') {
		str = String_UNSAFE_SubstringAt(&str, 2);
	}
	len = String_EncodeUtf8(dst->buffer, &str);
	dst->buffer[len] = '\0';
}

void Platform_DecodePath(cc_string* dst, const cc_filepath* path) {
	String_AppendConst(dst, path->buffer);
}

void Directory_GetCachePath(cc_string* path) { }

static cc_result Amiga_Error(void) {
	LONG err = IoErr();
	return err ? (cc_result)err : ERROR_OBJECT_NOT_FOUND;
}

cc_result Directory_Create2(const cc_filepath* path) {
	BPTR lock = CreateDir((CONST_STRPTR)path->buffer);
	if (!lock) return Amiga_Error();
	UnLock(lock);
	return 0;
}

/* -1: nothing there, 0: a file, 1: a drawer */
static int Amiga_Kind(const char* path) {
	struct FileInfoBlock* fib;
	BPTR lock;
	int kind = -1;

	lock = Lock((CONST_STRPTR)path, ACCESS_READ);
	if (!lock) return -1;

	fib = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL);
	if (fib) {
		if (Examine(lock, fib)) kind = fib->fib_DirEntryType > 0 ? 1 : 0;
		FreeDosObject(DOS_FIB, fib);
	}
	UnLock(lock);
	return kind;
}

int File_Exists(const cc_filepath* path) {
	return Amiga_Kind(path->buffer) == 0;
}

cc_result Directory_Enum(const cc_string* dirPath, void* obj, Directory_EnumCallback callback) {
	cc_string path; char pathBuffer[FILENAME_SIZE];
	struct FileInfoBlock* fib;
	cc_filepath str;
	cc_result res = 0;
	BPTR lock;
	int len;

	Platform_EncodePath(&str, dirPath);
	lock = Lock((CONST_STRPTR)str.buffer, ACCESS_READ);
	if (!lock) return Amiga_Error();

	fib = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL);
	if (!fib) { UnLock(lock); return ERR_OUT_OF_MEMORY; }

	if (!Examine(lock, fib)) {
		res = Amiga_Error();
	} else if (fib->fib_DirEntryType <= 0) {
		res = ERROR_OBJECT_WRONG_TYPE;
	} else {
		String_InitArray(path, pathBuffer);

		while (ExNext(lock, fib)) {
			path.length = 0;
			String_Format1(&path, "%s/", dirPath);

			len = String_Length((const char*)fib->fib_FileName);
			String_AppendUtf8(&path, (const char*)fib->fib_FileName, len);
			callback(&path, obj, fib->fib_DirEntryType > 0);
		}
		res = IoErr();
		if (res == ERROR_NO_MORE_ENTRIES) res = 0;
	}

	FreeDosObject(DOS_FIB, fib);
	UnLock(lock);
	return res;
}

static cc_result File_Do(cc_file* file, const char* path, LONG mode) {
	BPTR fh = Open((CONST_STRPTR)path, mode);
	*file = (cc_file)fh;
	return fh ? 0 : Amiga_Error();
}

cc_result File_Open(cc_file* file, const cc_filepath* path) {
	return File_Do(file, path->buffer, MODE_OLDFILE);
}

cc_result File_Create(cc_file* file, const cc_filepath* path) {
	return File_Do(file, path->buffer, MODE_NEWFILE);
}

cc_result File_OpenOrCreate(cc_file* file, const cc_filepath* path) {
	return File_Do(file, path->buffer, MODE_READWRITE);
}

cc_result File_Read(cc_file file, void* data, cc_uint32 count, cc_uint32* bytesRead) {
	LONG n = Read((BPTR)file, data, count);
	if (n < 0) { *bytesRead = 0; return Amiga_Error(); }
	*bytesRead = (cc_uint32)n;
	return 0;
}

cc_result File_Write(cc_file file, const void* data, cc_uint32 count, cc_uint32* bytesWrote) {
	LONG n = Write((BPTR)file, (APTR)data, count);
	if (n < 0) { *bytesWrote = 0; return Amiga_Error(); }
	*bytesWrote = (cc_uint32)n;
	return 0;
}

cc_result File_Close(cc_file file) {
	return Close((BPTR)file) ? 0 : Amiga_Error();
}

cc_result File_Seek(cc_file file, int offset, int seekType) {
	static const LONG modes[3] = { OFFSET_BEGINNING, OFFSET_CURRENT, OFFSET_END };
	SetIoErr(0);
	if (Seek((BPTR)file, offset, modes[seekType]) == -1 && IoErr()) return IoErr();
	return 0;
}

cc_result File_Position(cc_file file, cc_uint32* pos) {
	LONG p;
	SetIoErr(0);
	p = Seek((BPTR)file, 0, OFFSET_CURRENT);
	if (p == -1 && IoErr()) return IoErr();
	*pos = (cc_uint32)p;
	return 0;
}

cc_result File_Length(cc_file file, cc_uint32* len) {
	struct FileInfoBlock* fib = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL);
	cc_result res = 0;
	if (!fib) return ERR_OUT_OF_MEMORY;

	if (ExamineFH((BPTR)file, fib)) {
		*len = (cc_uint32)fib->fib_Size;
	} else {
		res = Amiga_Error();
	}
	FreeDosObject(DOS_FIB, fib);
	return res;
}


/*########################################################################################################################*
*--------------------------------------------------------Threading--------------------------------------------------------*
*#########################################################################################################################*/
void Thread_Sleep(cc_uint32 milliseconds) {
	/* Delay() counts in 50ths of a second; a shorter wait still yields */
	cc_uint32 ticks = (milliseconds + 19) / 20;
	if (ticks) Delay(ticks);
}


/*########################################################################################################################*
*-----------------------------------------------------Process/Module------------------------------------------------------*
*#########################################################################################################################*/
int Platform_GetCommandLineArgs(int argc, STRING_REF char** argv, cc_string* args) {
	int i, count;
	if (gameHasArgs) return GetGameArgs(args);

	/* argc is 0 when started from the Workbench */
	count = argc > 1 ? argc - 1 : 0;
	if (count > GAME_MAX_CMDARGS) count = GAME_MAX_CMDARGS;

	for (i = 0; i < count; i++) {
		args[i] = String_FromReadonly(argv[i + 1]);
	}
	/* No launcher (no server list, no updates, no downloads): singleplayer */
	if (!count) {
		args[0] = String_FromReadonly(DEFAULT_SINGLEPLAYER_ARG);
		count   = 1;
	}
	return count;
}

/* The game's files (options.txt, texpacks/, maps/, screenshots/) live */
/*  beside the program, whichever drawer it was started from */
cc_result Platform_SetDefaultCurrentDirectory(void) {
	BPTR dir = GetProgramDir();
	BPTR ours;
	if (!dir) return 0;

	ours = DupLock(dir);
	if (!ours) return Amiga_Error();
	amiga_oldDir     = CurrentDir(ours);
	amiga_dirChanged = true;
	return 0;
}

cc_result Process_StartGame2(const cc_string* args, int numArgs) {
	return SetGameArgs(args, numArgs);
}

void Process_Exit(cc_result code) {
	Amiga_Cleanup();
	exit(code ? RETURN_FAIL : RETURN_OK);
}


/*########################################################################################################################*
*--------------------------------------------------------Platform---------------------------------------------------------*
*#########################################################################################################################*/
void Platform_Free(void) { }

cc_bool Platform_DescribeError(cc_result res, cc_string* dst) {
	char buf[128];
	int len;
	if (res >= 0x10000 || !Fault(res, NULL, (STRPTR)buf, sizeof(buf))) return false;

	len = String_Length(buf);
	String_AppendUtf8(dst, buf, len);
	return len > 0;
}

void Platform_Init(void) { }

#endif

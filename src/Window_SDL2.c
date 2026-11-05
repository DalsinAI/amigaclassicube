#include "Core.h"
#if CC_WIN_BACKEND == CC_WIN_BACKEND_SDL2
#undef CC_BUILD_EGL /* eglCreateWindowSurface can't use an SDL window */
#include "_WindowBase.h"
#include "Graphics.h"
#include "String_.h"
#include "Funcs.h"
#include "Bitmap.h"
#include "Errors.h"
#include <SDL2/SDL.h>

static SDL_Window* win_handle;

#ifdef CC_BUILD_AMIGA_RTG
/* AmigaOS 3.x on OpenGPU's SDL2.module: a full-screen RTG screen of
   AMIGA_SCREEN_W x AMIGA_SCREEN_H (or a Workbench window with the option
   amiga-windowed=true), a renderer, and a streaming XRGB8888 texture of
   the size the game draws at (window-width x window-height, 640x480 unless
   set lower), which the renderer scales up to the screen. The window
   surface is not used: SDL2.module reports a 24-bit one. */
#include "Options.h"
/* amiga/amiga_dialog.c: Intuition's headers are kept out of this file */
/*  (the NDK has a FILENAME_SIZE of its own) */
void amiga_show_requester(const char* title, const char* msg, int over_game);

#define AMIGA_SCREEN_W 640
#define AMIGA_SCREEN_H 480
static SDL_Renderer* amiga_renderer;
static SDL_Texture*  amiga_texture;
static int amiga_w = AMIGA_SCREEN_W, amiga_h = AMIGA_SCREEN_H;
#endif

#ifdef CC_BUILD_OS2
#define INCL_PM
#include <os2.h>
// Internal OS/2 driver data
typedef struct _WINDATA {
    SDL_Window     *window;
    void					 *pOutput; /* Video output routines */
    HWND            hwndFrame;
    HWND            hwnd;
    PFNWP           fnUserWndProc;
    PFNWP           fnWndFrameProc;

    void         		*pVOData; /* Video output data */

    HRGN            hrgnShape;
    HPOINTER        hptrIcon;
    RECTL           rectlBeforeFS;

    LONG            lSkipWMSize;
    LONG            lSkipWMMove;
    LONG            lSkipWMMouseMove;
    LONG            lSkipWMVRNEnabled;
    LONG            lSkipWMAdjustFramePos;
} WINDATA;
#endif


static void SetGLAttributes();

static void RefreshWindowBounds(void) {
#ifdef CC_BUILD_AMIGA_RTG
	/* The game's coordinates are the texture's; the renderer's logical size */
	/*  scales the mouse to them */
	Window_Main.Width  = amiga_w;
	Window_Main.Height = amiga_h;
#else
	SDL_GetWindowSize(win_handle, &Window_Main.Width, &Window_Main.Height);
#endif
}

static void Window_SDLFail(const char* place) {
	char strBuffer[256];
	cc_string str;
	String_InitArray_NT(str, strBuffer);

	String_Format2(&str, "Error when %c: %c", place, SDL_GetError());
	str.buffer[str.length] = '\0';
	Process_Abort(str.buffer);
}

void Window_PreInit(void) {
#ifdef CC_BUILD_AMIGA_RTG
	/* A missing joystick driver must not take the screen with it */
	SDL_Init(SDL_INIT_VIDEO);
	SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
#else
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
#endif
	DisplayInfo.CursorVisible = true;
}

#ifdef CC_BUILD_AMIGA_RTG
void Window_Init(void) {
	Input.Sources = INPUT_SOURCE_NORMAL;

	DisplayInfo.Width  = AMIGA_SCREEN_W;
	DisplayInfo.Height = AMIGA_SCREEN_H;
	DisplayInfo.Depth  = 32;
	DisplayInfo.ScaleX = 1;
	DisplayInfo.ScaleY = 1;
}
#else
void Window_Init(void) {
	SDL_DisplayMode mode = { 0 };
	SDL_GetDesktopDisplayMode(0, &mode);
	Input.Sources = INPUT_SOURCE_NORMAL;

	DisplayInfo.Width  = mode.w;
	DisplayInfo.Height = mode.h;
	DisplayInfo.Depth  = SDL_BITSPERPIXEL(mode.format);
	DisplayInfo.ScaleX = 1;
	DisplayInfo.ScaleY = 1;
}
#endif

void Window_Free(void) { }


#ifndef CC_BUILD_AMIGA_RTG
/* See misc/sdl/sdl_icon_gen.cs for how to generate this file */
#include "../misc/sdl/CCIcon_SDL.h"

static void ApplyIcon(void) {
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom((void*)CCIcon_Data, CCIcon_Width, CCIcon_Height, 32, CCIcon_Pitch,
													0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	SDL_SetWindowIcon(win_handle, surface);
}
#endif

#ifdef CC_BUILD_AMIGA_RTG
static void DoCreateWindow(int width, int height, int flags) {
	cc_bool windowed = Options_GetBool("amiga-windowed", false);
	if (width  < 160 || width  > AMIGA_SCREEN_W) width  = AMIGA_SCREEN_W;
	if (height < 120 || height > AMIGA_SCREEN_H) height = AMIGA_SCREEN_H;
	amiga_w = width; amiga_h = height;

	if (windowed) {
		win_handle = SDL_CreateWindow("ClassiCube", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
						width, height, 0);
	} else {
		win_handle = SDL_CreateWindow("ClassiCube", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
						AMIGA_SCREEN_W, AMIGA_SCREEN_H, SDL_WINDOW_FULLSCREEN);
	}
	if (!win_handle) Window_SDLFail("creating window");

	amiga_renderer = SDL_CreateRenderer(win_handle, -1, 0);
	if (!amiga_renderer) Window_SDLFail("creating renderer");
	SDL_RenderSetLogicalSize(amiga_renderer, width, height);

	RefreshWindowBounds();
	Window_Main.Exists     = true;
	Window_Main.Handle.ptr = win_handle;
	Window_Main.Focused    = true;
	Window_Main.UIScaleX   = DEFAULT_UI_SCALE_X;
	Window_Main.UIScaleY   = DEFAULT_UI_SCALE_Y;
	Window_Main.SoftKeyboardInstant = true;
}
#else
static void DoCreateWindow(int width, int height, int flags) {
	win_handle = SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, 
					flags | SDL_WINDOW_RESIZABLE);
	if (!win_handle) Window_SDLFail("creating window");

	RefreshWindowBounds();
	Window_Main.Exists     = true;
	Window_Main.Handle.ptr = win_handle;
	Window_Main.UIScaleX   = DEFAULT_UI_SCALE_X;
	Window_Main.UIScaleY   = DEFAULT_UI_SCALE_Y;

	Window_Main.SoftKeyboardInstant = true;
	ApplyIcon();
	/* TODO grab using SDL_SetWindowGrab? seems to be unnecessary on Linux at least */
}
#endif

void Window_Create2D(int width, int height) { DoCreateWindow(width, height, 0); }
#if CC_GFX_BACKEND_IS_GL()
void Window_Create3D(int width, int height) {
	SetGLAttributes();
	DoCreateWindow(width, height, SDL_WINDOW_OPENGL);
}
#else
void Window_Create3D(int width, int height) { DoCreateWindow(width, height, 0); }
#endif

void Window_Destroy(void) {
#ifdef CC_BUILD_AMIGA_RTG
	if (amiga_texture)  SDL_DestroyTexture(amiga_texture);
	if (amiga_renderer) SDL_DestroyRenderer(amiga_renderer);
	amiga_texture  = NULL;
	amiga_renderer = NULL;
#endif
	SDL_DestroyWindow(win_handle);
	win_handle = NULL;
}

void Window_SetTitle(const cc_string* title) {
	char str[NATIVE_STR_LEN];
	String_EncodeUtf8(str, title);
	SDL_SetWindowTitle(win_handle, str);
}

void Clipboard_GetText(cc_string* value) {
	char* ptr = SDL_GetClipboardText();
	if (!ptr) return;

	int len = String_Length(ptr);
	String_AppendUtf8(value, ptr, len);
	SDL_free(ptr);
}

void Clipboard_SetText(const cc_string* value) {
	char str[NATIVE_STR_LEN];
	String_EncodeUtf8(str, value);
	SDL_SetClipboardText(str);
}

int Window_GetWindowState(void) {
	Uint32 flags = SDL_GetWindowFlags(win_handle);

	if (flags & SDL_WINDOW_MINIMIZED)          return WINDOW_STATE_MINIMISED;
	if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) return WINDOW_STATE_FULLSCREEN;
	return WINDOW_STATE_NORMAL;
}

#ifdef CC_BUILD_AMIGA_RTG
/* The screen (or window) is chosen once, at start-up */
cc_result Window_EnterFullscreen(void) { return 0; }
cc_result Window_ExitFullscreen(void)  { return 0; }
#else
cc_result Window_EnterFullscreen(void) {
	return SDL_SetWindowFullscreen(win_handle, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

cc_result Window_ExitFullscreen(void) { 
	return SDL_SetWindowFullscreen(win_handle, 0);
}
#endif

int Window_IsObscured(void) { return 0; }

void Window_Show(void) { SDL_ShowWindow(win_handle); }

void Window_SetSize(int width, int height) {
#ifndef CC_BUILD_AMIGA_RTG
	SDL_SetWindowSize(win_handle, width, height);
#endif
}

void Window_RequestClose(void) {
	SDL_Event e;
	e.type = SDL_QUIT;
	SDL_PushEvent(&e);
}

static int MapNativeKey(SDL_Keycode k) {
	if (k >= SDLK_0   && k <= SDLK_9)   { return '0'     + (k - SDLK_0); }
	if (k >= SDLK_a   && k <= SDLK_z)   { return 'A'     + (k - SDLK_a); }
	if (k >= SDLK_F1  && k <= SDLK_F12) { return CCKEY_F1  + (k - SDLK_F1); }
	if (k >= SDLK_F13 && k <= SDLK_F24) { return CCKEY_F13 + (k - SDLK_F13); }
	/* SDLK_KP_0 isn't before SDLK_KP_1 */
	if (k >= SDLK_KP_1 && k <= SDLK_KP_9) { return CCKEY_KP1 + (k - SDLK_KP_1); }

	switch (k) {
		case SDLK_RETURN: return CCKEY_ENTER;
		case SDLK_ESCAPE: return CCKEY_ESCAPE;
		case SDLK_BACKSPACE: return CCKEY_BACKSPACE;
		case SDLK_TAB:    return CCKEY_TAB;
		case SDLK_SPACE:  return CCKEY_SPACE;
		case SDLK_QUOTE:  return CCKEY_QUOTE;
		case SDLK_EQUALS: return CCKEY_EQUALS;
		case SDLK_COMMA:  return CCKEY_COMMA;
		case SDLK_MINUS:  return CCKEY_MINUS;
		case SDLK_PERIOD: return CCKEY_PERIOD;
		case SDLK_SLASH:  return CCKEY_SLASH;
		case SDLK_SEMICOLON:    return CCKEY_SEMICOLON;
		case SDLK_LEFTBRACKET:  return CCKEY_LBRACKET;
		case SDLK_BACKSLASH:    return CCKEY_BACKSLASH;
		case SDLK_RIGHTBRACKET: return CCKEY_RBRACKET;
		case SDLK_BACKQUOTE:    return CCKEY_TILDE;
		case SDLK_CAPSLOCK:     return CCKEY_CAPSLOCK;
		case SDLK_PRINTSCREEN: return CCKEY_PRINTSCREEN;
		case SDLK_SCROLLLOCK:  return CCKEY_SCROLLLOCK;
		case SDLK_PAUSE:       return CCKEY_PAUSE;
		case SDLK_INSERT:   return CCKEY_INSERT;
		case SDLK_HOME:     return CCKEY_HOME;
		case SDLK_PAGEUP:   return CCKEY_PAGEUP;
		case SDLK_DELETE:   return CCKEY_DELETE;
		case SDLK_END:      return CCKEY_END;
		case SDLK_PAGEDOWN: return CCKEY_PAGEDOWN;
		case SDLK_RIGHT: return CCKEY_RIGHT;
		case SDLK_LEFT:  return CCKEY_LEFT;
		case SDLK_DOWN:  return CCKEY_DOWN;
		case SDLK_UP:    return CCKEY_UP;

		case SDLK_NUMLOCKCLEAR: return CCKEY_NUMLOCK;
		case SDLK_KP_DIVIDE: return CCKEY_KP_DIVIDE;
		case SDLK_KP_MULTIPLY: return CCKEY_KP_MULTIPLY;
		case SDLK_KP_MINUS: return CCKEY_KP_MINUS;
		case SDLK_KP_PLUS: return CCKEY_KP_PLUS;
		case SDLK_KP_ENTER: return CCKEY_KP_ENTER;
		case SDLK_KP_0: return CCKEY_KP0;
		case SDLK_KP_PERIOD: return CCKEY_KP_DECIMAL;

		case SDLK_LCTRL: return CCKEY_LCTRL;
		case SDLK_LSHIFT: return CCKEY_LSHIFT;
		case SDLK_LALT: return CCKEY_LALT;
		case SDLK_LGUI: return CCKEY_LWIN;
		case SDLK_RCTRL: return CCKEY_RCTRL;
		case SDLK_RSHIFT: return CCKEY_RSHIFT;
		case SDLK_RALT: return CCKEY_RALT;
		case SDLK_RGUI: return CCKEY_RWIN;
		
		case SDLK_AUDIONEXT: return CCKEY_MEDIA_NEXT;
		case SDLK_AUDIOPREV: return CCKEY_MEDIA_PREV;
		case SDLK_AUDIOPLAY: return CCKEY_MEDIA_PLAY;
		case SDLK_AUDIOSTOP: return CCKEY_MEDIA_STOP;
		
		case SDLK_AUDIOMUTE:  return CCKEY_VOLUME_MUTE;
		case SDLK_VOLUMEDOWN: return CCKEY_VOLUME_DOWN;
		case SDLK_VOLUMEUP:   return CCKEY_VOLUME_UP;
	}
	return INPUT_NONE;
}

static void OnKeyEvent(const SDL_Event* e) {
	cc_bool pressed = e->key.state == SDL_PRESSED;
	int key = MapNativeKey(e->key.keysym.sym);
	if (key) Input_Set(key, pressed);
}

static void OnMouseEvent(const SDL_Event* e) {
	cc_bool pressed = e->button.state == SDL_PRESSED;
	int btn;
	switch (e->button.button) {
		case SDL_BUTTON_LEFT:   btn = CCMOUSE_L; break;
		case SDL_BUTTON_MIDDLE: btn = CCMOUSE_M; break;
		case SDL_BUTTON_RIGHT:  btn = CCMOUSE_R; break;
		case SDL_BUTTON_X1:     btn = CCMOUSE_X1; break;
		case SDL_BUTTON_X2:     btn = CCMOUSE_X2; break;
		default: return;
	}
	Input_Set(btn, pressed);
}

static void OnTextEvent(const SDL_Event* e) {
	cc_codepoint cp;
	const char* src;
	int i, len;

	src = e->text.text;
	len = String_CalcLen(src, SDL_TEXTINPUTEVENT_TEXT_SIZE);

	while (len > 0) {
		i = Convert_Utf8ToCodepoint(&cp, src, len);
		if (!i) break;

		Event_RaiseInt(&InputEvents.Press, cp);
		src += i; len -= i;
	}
}

static void OnWindowEvent(const SDL_Event* e) {
	switch (e->window.event) {
		case SDL_WINDOWEVENT_EXPOSED:
			Event_RaiseVoid(&WindowEvents.RedrawNeeded);
			break;
		case SDL_WINDOWEVENT_SIZE_CHANGED:
#ifdef CC_BUILD_AMIGA_RTG
			break; /* the texture keeps its size; the renderer scales it */
#endif
			RefreshWindowBounds();
			Event_RaiseVoid(&WindowEvents.Resized);
			break;
		case SDL_WINDOWEVENT_MINIMIZED:
		case SDL_WINDOWEVENT_MAXIMIZED:
		case SDL_WINDOWEVENT_RESTORED:
			Event_RaiseVoid(&WindowEvents.StateChanged);
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			Window_Main.Focused = true;
			Event_RaiseVoid(&WindowEvents.FocusChanged);
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			Window_Main.Focused = false;
			Event_RaiseVoid(&WindowEvents.FocusChanged);
			break;
		case SDL_WINDOWEVENT_CLOSE:
			Window_RequestClose();
			break;
		}
}

void Window_ProcessEvents(float delta) {
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {

		case SDL_KEYDOWN:
		case SDL_KEYUP:
			OnKeyEvent(&e); break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			OnMouseEvent(&e); break;
		case SDL_MOUSEWHEEL:
			Mouse_ScrollHWheel(e.wheel.x);
			Mouse_ScrollVWheel(e.wheel.y);
			break;
		case SDL_MOUSEMOTION:
			Pointer_SetPosition(0, e.motion.x, e.motion.y);
			if (Input.RawMode) Event_RaiseRawMove(&PointerEvents.RawMoved, e.motion.xrel, e.motion.yrel);
			break;
		case SDL_TEXTINPUT:
			OnTextEvent(&e); break;
		case SDL_WINDOWEVENT:
			OnWindowEvent(&e); break;

		case SDL_QUIT:
			Window_Main.Exists = false;
			Event_RaiseVoid(&WindowEvents.Closing);
			break;

		case SDL_RENDER_DEVICE_RESET:
			Gfx_LoseContext("SDL device reset event");
			Gfx_RecreateContext();
			break;
		}
	}
}

#ifdef CC_BUILD_AMIGA_RTG
/* SDL's mouse state and warp are in the window's pixels, the game's in the */
/*  texture's (smaller when window-width/height are set below the screen's) */
static void Amiga_WindowSize(int* w, int* h) {
	SDL_GetWindowSize(win_handle, w, h);
	if (*w <= 0) *w = amiga_w;
	if (*h <= 0) *h = amiga_h;
}

static void Cursor_GetRawPos(int* x, int* y) {
	int w, h;
	SDL_GetMouseState(x, y);
	Amiga_WindowSize(&w, &h);
	*x = *x * amiga_w / w;
	*y = *y * amiga_h / h;
}
void Cursor_SetPosition(int x, int y) {
	int w, h;
	Amiga_WindowSize(&w, &h);
	SDL_WarpMouseInWindow(win_handle, x * w / amiga_w, y * h / amiga_h);
}
#else
static void Cursor_GetRawPos(int* x, int* y) {
	SDL_GetMouseState(x, y);
}
void Cursor_SetPosition(int x, int y) {
	SDL_WarpMouseInWindow(win_handle, x, y);
}
#endif

static void Cursor_DoSetVisible(cc_bool visible) {
	SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}

#ifdef CC_BUILD_AMIGA_RTG
static void ShowDialogCore(const char* title, const char* msg) {
	Platform_LogConst(title);
	Platform_LogConst(msg);
	amiga_show_requester(title, msg, win_handle != NULL);
}
#else
static void ShowDialogCore(const char* title, const char* msg) {
	SDL_ShowSimpleMessageBox(0, title, msg, win_handle);
}
#endif

cc_result Window_OpenFileDialog(const struct OpenFileDialogArgs* args) {
#if defined CC_BUILD_OS2
	FILEDLG fileDialog;
	HWND hDialog;

	memset(&fileDialog, 0, sizeof(FILEDLG));
	fileDialog.cbSize = sizeof(FILEDLG);
	fileDialog.fl = FDS_HELPBUTTON | FDS_CENTER | FDS_PRELOAD_VOLINFO | FDS_OPEN_DIALOG;
	fileDialog.pszTitle = args->description;
	fileDialog.pszOKButton = NULL;
	fileDialog.pfnDlgProc = WinDefFileDlgProc;

	Mem_Copy(fileDialog.szFullFile, *args->filters, CCHMAXPATH);
	hDialog = WinFileDlg(HWND_DESKTOP, 0, &fileDialog);
	if (fileDialog.lReturn == DID_OK) {
		cc_string temp = String_FromRaw(fileDialog.szFullFile, CCHMAXPATH); 
		args->Callback(&temp);
	}
	
	return 0;
#else
	return ERR_NOT_SUPPORTED;
#endif
}

cc_result Window_SaveFileDialog(const struct SaveFileDialogArgs* args) {
#if defined CC_BUILD_OS2
	FILEDLG fileDialog;
	HWND hDialog;

	memset(&fileDialog, 0, sizeof(FILEDLG));
	fileDialog.cbSize = sizeof(FILEDLG);
	fileDialog.fl = FDS_HELPBUTTON | FDS_CENTER | FDS_PRELOAD_VOLINFO | FDS_SAVEAS_DIALOG;
	fileDialog.pszTitle = args->titles;
	fileDialog.pszOKButton = NULL;
	fileDialog.pfnDlgProc = WinDefFileDlgProc;

	Mem_Copy(fileDialog.szFullFile, *args->filters, CCHMAXPATH);
	hDialog = WinFileDlg(HWND_DESKTOP, 0, &fileDialog);
	if (fileDialog.lReturn == DID_OK) {
		cc_string temp = String_FromRaw(fileDialog.szFullFile, CCHMAXPATH);
		args->Callback(&temp);
	}
	
	return 0;
#else
	return ERR_NOT_SUPPORTED;
#endif
}

#ifdef CC_BUILD_AMIGA_RTG
void Window_AllocFramebuffer(struct Bitmap* bmp, int width, int height) {
	/* BitmapCol is 0xAARRGGBB in a 32-bit word: XRGB8888 for SDL */
	amiga_texture = SDL_CreateTexture(amiga_renderer, SDL_PIXELFORMAT_RGB888,
					SDL_TEXTUREACCESS_STREAMING, width, height);
	if (!amiga_texture) Window_SDLFail("creating texture");

	bmp->scan0  = (BitmapCol*)Mem_Alloc(width * height, BITMAPCOLOR_SIZE, "window pixels");
	bmp->width  = width;
	bmp->height = height;
}

void Window_DrawFramebuffer(Rect2D r, struct Bitmap* bmp) {
	SDL_UpdateTexture(amiga_texture, NULL, bmp->scan0, bmp->width * BITMAPCOLOR_SIZE);
	SDL_RenderClear(amiga_renderer);
	SDL_RenderCopy(amiga_renderer, amiga_texture, NULL, NULL);
	SDL_RenderPresent(amiga_renderer);
}

void Window_FreeFramebuffer(struct Bitmap* bmp) {
	if (amiga_texture) SDL_DestroyTexture(amiga_texture);
	amiga_texture = NULL;
	Mem_Free(bmp->scan0);
	bmp->scan0 = NULL;
}
#else
static SDL_Surface* win_surface;
static SDL_Surface* blit_surface;

void Window_AllocFramebuffer(struct Bitmap* bmp, int width, int height) {
	SDL_PixelFormat* fmt;
	win_surface = SDL_GetWindowSurface(win_handle);
	if (!win_surface) Window_SDLFail("getting window surface");

	fmt = win_surface->format;
	if (fmt->BitsPerPixel != 32) {
		/* Slow path: e.g. 15 or 16 bit pixels */
		Platform_Log1("Slow color depth: %b bpp", &fmt->BitsPerPixel);
		blit_surface = SDL_CreateRGBSurface(0, win_surface->w, win_surface->h, 32, 0, 0, 0, 0);
		if (!blit_surface) Window_SDLFail("creating blit surface");

		SDL_SetSurfaceBlendMode(blit_surface, SDL_BLENDMODE_NONE);
		bmp->scan0 = blit_surface->pixels;
	} else {
		/* Fast path: 32 bit pixels */
		if (SDL_MUSTLOCK(win_surface)) {
			int ret = SDL_LockSurface(win_surface);
			if (ret < 0) Window_SDLFail("locking window surface");
		}
		bmp->scan0 = win_surface->pixels;
	}
	/* TODO proper stride */
	bmp->width  = width;
	bmp->height = height;
}

void Window_DrawFramebuffer(Rect2D r, struct Bitmap* bmp) {
	SDL_Rect rect;
	rect.x = r.x; rect.w = r.width;
	rect.y = r.y; rect.h = r.height;

	if (blit_surface) SDL_BlitSurface(blit_surface, &rect, win_surface, &rect);
	SDL_UpdateWindowSurfaceRects(win_handle, &rect, 1);
}

void Window_FreeFramebuffer(struct Bitmap* bmp) {
	if (blit_surface) SDL_FreeSurface(blit_surface);
	blit_surface = NULL;

	/* SDL docs explicitly say to NOT free window surface */
	/* https://wiki.libsdl.org/SDL_GetWindowSurface */
	/* TODO: Do we still need to unlock it though? */
}
#endif

void OnscreenKeyboard_Open(struct OpenKeyboardArgs* args) { SDL_StartTextInput(); }
void OnscreenKeyboard_SetText(const cc_string* text) { }
void OnscreenKeyboard_Close(void) { SDL_StopTextInput(); }

void Window_EnableRawMouse(void) {
	RegrabMouse();
	SDL_SetRelativeMouseMode(true);
	Input.RawMode = true;
}
void Window_UpdateRawMouse(void) { CentreMousePosition(); }

void Window_DisableRawMouse(void) {
	RegrabMouse();
	SDL_SetRelativeMouseMode(false);
	Input.RawMode = false;
}


/*########################################################################################################################*
*--------------------------------------------------------Gamepads---------------------------------------------------------*
*#########################################################################################################################*/
#include "ExtMath.h"
static SDL_GameController* controllers[INPUT_MAX_GAMEPADS];

void Gamepads_PreInit(void) { }

void Gamepads_Init(void) {
    for (int i = 0, j = 0; i < SDL_NumJoysticks() && j < INPUT_MAX_GAMEPADS; i++) 
	{
        if (!SDL_IsGameController(i)) continue;
		controllers[j++] = SDL_GameControllerOpen(i);
    }
}

static void ProcessGamepadButtons(int port, SDL_GameController* gp) {
	Gamepad_SetButton(port, CCPAD_1, SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_A));
	Gamepad_SetButton(port, CCPAD_2, SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_B));
	Gamepad_SetButton(port, CCPAD_3, SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_X));
	Gamepad_SetButton(port, CCPAD_4, SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_Y));

	Gamepad_SetButton(port, CCPAD_ZL, SDL_GameControllerGetAxis(  gp, SDL_CONTROLLER_AXIS_TRIGGERLEFT ) > 8000);
	Gamepad_SetButton(port, CCPAD_ZR, SDL_GameControllerGetAxis(  gp, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8000);
	Gamepad_SetButton(port, CCPAD_L,  SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
	Gamepad_SetButton(port, CCPAD_R,  SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));

	Gamepad_SetButton(port, CCPAD_SELECT, SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_GUIDE));
	Gamepad_SetButton(port, CCPAD_START,  SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_START));
	Gamepad_SetButton(port, CCPAD_LSTICK, SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_LEFTSTICK));
	Gamepad_SetButton(port, CCPAD_RSTICK, SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_RIGHTSTICK));
	
	Gamepad_SetButton(port, CCPAD_UP,    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_DPAD_UP));
	Gamepad_SetButton(port, CCPAD_DOWN,  SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_DPAD_DOWN));
	Gamepad_SetButton(port, CCPAD_LEFT,  SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_DPAD_LEFT));
	Gamepad_SetButton(port, CCPAD_RIGHT, SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_DPAD_RIGHT));
}

#define PAD_AXIS_SCALE 32768.0f
static void ProcessJoystick(int port, SDL_GameController* gp, int axis, float delta) {
	int x = SDL_GameControllerGetAxis(gp, axis == PAD_AXIS_LEFT ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX);
	int y = SDL_GameControllerGetAxis(gp, axis == PAD_AXIS_LEFT ? SDL_CONTROLLER_AXIS_LEFTY : SDL_CONTROLLER_AXIS_RIGHTY);

	// May not be exactly 0 on actual hardware
	if (Math_AbsI(x) <= 1024) x = 0;
	if (Math_AbsI(y) <= 1024) y = 0;
	
	Gamepad_SetAxis(port, axis, x / PAD_AXIS_SCALE, -y / PAD_AXIS_SCALE, delta);
}

void Gamepads_Process(float delta) {
	for (int i = 0; i < INPUT_MAX_GAMEPADS; i++)
	{
		SDL_GameController* gp = controllers[i];
		if (!gp) continue;
		int port = Gamepad_Connect(0x5D12 + i, PadBind_Defaults);

		ProcessGamepadButtons(port, gp);
		ProcessJoystick(port, gp, PAD_AXIS_LEFT,  delta);
		ProcessJoystick(port, gp, PAD_AXIS_RIGHT, delta);
	}
}


/*########################################################################################################################*
*-----------------------------------------------------OpenGL context------------------------------------------------------*
*#########################################################################################################################*/
#if CC_GFX_BACKEND_IS_GL()
static SDL_GLContext win_ctx;

void SetGLAttributes(void) {
	struct GraphicsMode mode;
	InitGraphicsMode(&mode, DisplayInfo.Depth);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   mode.R);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, mode.G);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  mode.B);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);

	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   GLCONTEXT_DEFAULT_DEPTH);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);
#ifdef CC_BUILD_GLES
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
}

void GLContext_Create(void) {
	win_ctx = SDL_GL_CreateContext(win_handle);
	if (!win_ctx) Window_SDLFail("creating OpenGL context");
}

void GLContext_Update(void) { }
cc_bool GLContext_TryRestore(void) { return true; }
void GLContext_Free(void) {
	SDL_GL_DeleteContext(win_ctx);
	win_ctx = NULL;
}

void* GLContext_GetAddress(const char* function) {
	return SDL_GL_GetProcAddress(function);
}

cc_bool GLContext_SwapBuffers(void) {
	SDL_GL_SwapWindow(win_handle);
	return true;
}

void GLContext_SetVSync(cc_bool vsync) {
	SDL_GL_SetSwapInterval(vsync);
}
void GLContext_GetApiInfo(cc_string* info) { }

#endif
#endif

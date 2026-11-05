/* OpenGPU's SDL2.module (0.6) opens two screens for a SDL_WINDOW_FULLSCREEN
 * window and, when the window goes, leaves two empty ones behind (one of
 * them reopened at its 640x480 desktop mode); they stay in front of the
 * Workbench after the program has ended, grey, until the machine is reset.
 * The module never comes back to them, so after SDL_Quit() the game deals
 * with every screen that was not there before SDL started, has no window
 * left on it and SDL's title: closed if Intuition lets it, else sent behind
 * the Workbench.
 * From the Team's Seven Kingdoms port (amiga/amiga_screens.c). */
#include <exec/types.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <string.h>

extern struct IntuitionBase *IntuitionBase;

#define MAX_KNOWN 32
static struct Screen *known[MAX_KNOWN];
static int known_count = -1;

void amiga_screens_snapshot(void)
{
	struct Screen *s;
	ULONG lock = LockIBase(0);
	known_count = 0;
	for (s = IntuitionBase->FirstScreen; s && known_count < MAX_KNOWN; s = s->NextScreen)
		known[known_count++] = s;
	UnlockIBase(lock);
}

static int was_known(struct Screen *s)
{
	int i;
	for (i = 0; i < known_count; i++)
		if (known[i] == s)
			return 1;
	return 0;
}

void amiga_screens_close_leftovers(void)
{
	struct Screen *left[MAX_KNOWN];
	int n = 0, i;
	struct Screen *s;
	ULONG lock;

	if (known_count < 0)
		return;

	lock = LockIBase(0);
	for (s = IntuitionBase->FirstScreen; s && n < MAX_KNOWN; s = s->NextScreen)
		if (!was_known(s) && !s->FirstWindow && (s->Flags & SCREENTYPE) != WBENCHSCREEN
		    && s->Title && strcmp((const char *)s->Title, "SDL") == 0)
			left[n++] = s;
	UnlockIBase(lock);

	/* The module still counts itself a visitor of them, so CloseScreen()
	 * may refuse; then they go behind the Workbench at least. */
	for (i = 0; i < n; i++)
		if (!CloseScreen(left[i]))
			ScreenToBack(left[i]);
	if (n)
		WBenchToFront();
}

/* ClassiCube's message boxes on AmigaOS: SDL's is not to be relied on here
 * (and shows nothing once SDL has gone), so an Intuition requester on the
 * Workbench. While the game's screen is open the Workbench is brought to
 * the front for it and the game's screen put back in front afterwards.
 * BSD-3-Clause, like ClassiCube. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>

extern struct IntuitionBase *IntuitionBase;

void amiga_show_requester(const char *title, const char *msg, int over_game)
{
	struct EasyStruct es;
	struct Screen *front = over_game ? IntuitionBase->FirstScreen : NULL;

	es.es_StructSize   = sizeof(es);
	es.es_Flags        = 0;
	es.es_Title        = (UBYTE *)title;
	es.es_TextFormat   = (UBYTE *)"%s";
	es.es_GadgetFormat = (UBYTE *)"OK";

	if (front)
		WBenchToFront();
	EasyRequest(NULL, &es, NULL, (ULONG)msg);
	if (front)
		ScreenToFront(front);
}

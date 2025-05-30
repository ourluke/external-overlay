#include "overlay/overlay.h"

int main()
{
	Overlay overlay;
	overlay.findAttachWindow("Counter-Strike 2", "SDL_app");

	return 0;
}
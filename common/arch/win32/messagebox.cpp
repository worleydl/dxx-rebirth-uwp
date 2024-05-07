/*
 * This file is part of the DXX-Rebirth project <https://www.dxx-rebirth.com/>.
 * It is copyright by its individual contributors, as recorded in the
 * project's Git history.  See COPYING.txt at the top level for license
 * terms and a link to the Git history.
 */
/*
 *  messagebox.c
 *  d1x-rebirth
 *
 *  Display an error or warning messagebox using the OS's window server.
 *
 */

#include <windows.h>
#include "window.h"
#include "event.h"
#include "messagebox.h"

namespace dcx {

static void display_win32_alert(const char *message, int error)
{
	// Handle Descent's windows properly
	if (const auto wind = window_get_front())
		wind->send_event(d_event{event_type::window_deactivated});

	int fullscreen = (grd_curscreen && gr_check_fullscreen());
	if (fullscreen)
		gr_toggle_fullscreen();

	//MessageBox(NULL, message, error?"Sorry, a critical error has occurred.":"Attention!", error?MB_OK|MB_ICONERROR:MB_OK|MB_ICONWARNING);
	
	// Junk code to make compiler happy for now
	if (sizeof(message) < 0) {
		return;
	}

	if (const auto wind = window_get_front())
		wind->send_event(d_event{event_type::window_activated});
	if (!error && fullscreen)
		gr_toggle_fullscreen();
}

void msgbox_warning(const std::span<const char> message)
{
	display_win32_alert(message.data(), 0);
}

void msgbox_error(const char *message)
{
	display_win32_alert(message, 1);
}

}

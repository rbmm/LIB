#pragma once

#include "winz.h"

namespace CCursorCashe
{
	enum C_ID {
		ARROW, IBEAM, WAIT, CROSS, UPARROW, SIZENWSE, SIZENESW, SIZEWE, SIZENS, SIZEALL, NO, HAND, APPSTARTING, MAX
	};

	HCURSOR GetCursor(C_ID id);
};

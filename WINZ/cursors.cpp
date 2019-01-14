#include "StdAfx.h"

_NT_BEGIN

#include "cursors.h"

namespace CCursorCashe {

	HCURSOR hcur[MAX];

#define CASE(X) IDC_##X,

	HCURSOR GetCursor(C_ID id)
	{
		static PCWSTR _names[] = {
			CASE(ARROW)CASE(IBEAM)CASE(WAIT)CASE(CROSS)CASE(UPARROW)CASE(SIZENWSE)CASE(NO)
			CASE(SIZENESW)CASE(SIZEWE)CASE(SIZENS)CASE(SIZEALL)CASE(HAND)CASE(APPSTARTING)
		};

		if (id < RTL_NUMBER_OF(_names))
		{
			if (!hcur[id])
			{
				hcur[id] = LoadCursor(0, _names[id]);
			}

			return hcur[id];
		}

		return NULL;
	}
}

_NT_END
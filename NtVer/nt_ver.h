union NT_OS_VER 
{
	ULONG FullVersion;
	struct  
	{
		USHORT Build;
		union {
			USHORT Version;
			struct  
			{
				UCHAR Minor;
				UCHAR Major;
			};
		};
	};

	NT_OS_VER();
};

extern NT_OS_VER g_nt_ver;
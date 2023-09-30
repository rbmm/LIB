#pragma once

#ifndef _SAMOFFLINE_H
#define _SAMOFFLINE_H

#include "ntsam2.h"

EXTERN_C_START

// Windows::Rtl::IRtlSystemIsolationLayer
struct IRtlSystemIsolationLayer;

//////////////////////////////////////////////////////////////////////////
// Basic

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineCloseHandle(
					  _In_ SAM_HANDLE SamHandle
					  );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineFreeMemory(
					 _In_ PVOID Buffer
					 );

//////////////////////////////////////////////////////////////////////////
// Server

NTSYSAPI
NTSTATUS 
NTAPI 
SamOfflineConnect(
				  _In_ PCWSTR FileName, 
				  _Out_ PSAM_HANDLE ServerHandle
				  );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineConnectExternal(
						  _In_ PCWSTR,
						  _In_ PCWSTR,
						  _In_ PCWSTR,
						  _In_ PCWSTR,
						  _Out_ PSAM_HANDLE ServerHandle
						  );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineConnectForInstaller(
							  _In_ IRtlSystemIsolationLayer*,
							  _Out_ PSAM_HANDLE ServerHandle
							  );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineEnumerateDomainsInSamServer(
									  _In_ SAM_HANDLE ServerHandle,
									  _Inout_ PSAM_ENUMERATE_HANDLE EnumerationContext,
									  _Outptr_ PSAM_SID_ENUMERATION *Buffer, // 
									  _In_ ULONG PreferedMaximumLength,
									  _Out_ PULONG CountReturned
									  );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineLookupDomainInSamServer(
								  _In_ SAM_HANDLE ServerHandle,
								  _In_ PCUNICODE_STRING Name,
								  _Outptr_ PSID *DomainId
								  );

//////////////////////////////////////////////////////////////////////////
// Domain

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineOpenDomain(
					 _In_ SAM_HANDLE ServerHandle,
					 _In_ PSID DomainId,
					 _Out_ PSAM_HANDLE DomainHandle
					 );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineRidToSid(
				   _In_ SAM_HANDLE DomainHandle,
				   _In_ ULONG Rid,
				   _Outptr_ PSID *Sid
				   );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineLookupNamesInDomain(
							  _In_ SAM_HANDLE DomainHandle,
							  _In_ ULONG Count,
							  _In_reads_(Count) PCUNICODE_STRING Names,
							  _Out_ _Deref_post_count_(Count) PULONG *RelativeIds,
							  _Out_ _Deref_post_count_(Count) PSID_NAME_USE *Use
							  );

//////////////////////////////////////////////////////////////////////////
// Alias

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineEnumerateAliasesInDomain(
								   _In_ SAM_HANDLE DomainHandle,
								   _Inout_ PSAM_ENUMERATE_HANDLE EnumerationContext,
								   _Outptr_ PSAM_RID_ENUMERATION *Buffer, 
								   _In_ ULONG PreferedMaximumLength,
								   _Out_ PULONG CountReturned
								   );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineCreateAliasInDomain(
							  _In_ SAM_HANDLE DomainHandle,
							  _In_ PCUNICODE_STRING AccountName,
							  _Out_ PSAM_HANDLE AliasHandle,
							  _Out_ PULONG RelativeId
							  );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineOpenAlias(
					_In_ SAM_HANDLE DomainHandle,
					_In_ ULONG AliasId,
					_Out_ PSAM_HANDLE AliasHandle
					);

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineAddMemberToAlias(
						   _In_ SAM_HANDLE AliasHandle,
						   _In_ PSID MemberId
						   );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineGetMembersInAlias(
							_In_ SAM_HANDLE AliasHandle,
							_Out_ _Deref_post_count_(*MemberCount) PSID **MemberIds,
							_Out_ PULONG MemberCount
							);

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineDeleteAlias(
					  _In_ SAM_HANDLE AliasHandle
					  );



NTSYSAPI
NTSTATUS
NTAPI
SamOfflineQueryInformationAlias(
								_In_ SAM_HANDLE AliasHandle,
								_In_ ALIAS_INFORMATION_CLASS AliasInformationClass,
								_Outptr_ PVOID *Buffer
								);

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineRemoveMemberFromAlias(
								_In_ SAM_HANDLE AliasHandle,
								_In_ PSID MemberId
								);

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineSetInformationAlias(
							  _In_ SAM_HANDLE AliasHandle,
							  _In_ ALIAS_INFORMATION_CLASS AliasInformationClass,
							  _In_ PVOID Buffer
							  );

//////////////////////////////////////////////////////////////////////////
// User

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineCreateUserInDomain(
							 _In_ SAM_HANDLE DomainHandle,
							 _In_ PCUNICODE_STRING AccountName,
							 _Out_ PSAM_HANDLE UserHandle,
							 _Out_ PULONG RelativeId
							 );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineDeleteUser(
					 _In_ SAM_HANDLE UserHandle
					 );


NTSYSAPI
NTSTATUS
NTAPI
SamOfflineEnumerateUsersInDomain2(
								  _In_ SAM_HANDLE DomainHandle,
								  _Inout_ PSAM_ENUMERATE_HANDLE EnumerationContext,
								  _In_ ULONG UserAccountControl,
								  _Outptr_ PSAM_RID_ENUMERATION *Buffer, 
								  _In_ ULONG PreferedMaximumLength,
								  _Out_ PULONG CountReturned
								  );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineOpenUser(
				   _In_ SAM_HANDLE DomainHandle,
				   _In_ ULONG UserId,
				   _Out_ PSAM_HANDLE UserHandle
				   );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineQueryInformationUser(
							   _In_ SAM_HANDLE UserHandle,
							   _In_ USER_INFORMATION_CLASS UserInformationClass,
							   _Outptr_ PVOID *Buffer
							   );

NTSYSAPI
NTSTATUS
NTAPI
SamOfflineSetInformationUser(
							 _In_ SAM_HANDLE UserHandle,
							 _In_ USER_INFORMATION_CLASS UserInformationClass,
							 _In_ PVOID Buffer
							 );

EXTERN_C_END

#endif // _SAMOFFLINE_H

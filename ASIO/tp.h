#pragma once

namespace CThreadPool
{
  DWORD CALLBACK WorkThread(PVOID );
  void Stop();
  BOOL Start();
  NTSTATUS BindIoCompletionCallback(HANDLE FileHandle, LPOVERLAPPED_COMPLETION_ROUTINE Function);
  NTSTATUS Post(
	  VOID (WINAPI *Function)(NTSTATUS Status, ULONG_PTR Information, PVOID Context),
	  PVOID Context, 
	  NTSTATUS Status, 
	  ULONG_PTR Information);
};

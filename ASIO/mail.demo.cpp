#include "../asio/mailslot.h"

class CMailSlot : public MailSlot
{
  ULONG _dwThreadId;

  virtual void OnServerClosed()
  {
    PostThreadMessage(_dwThreadId, WM_QUIT, 0, 0);
  }
  
  ~CMailSlot()
  {
    DbgPrint("%x:%s<%p>\n", GetCurrentThreadId(), __FUNCTION__, this);
  }

public:
  CMailSlot() : _dwThreadId(GetCurrentThreadId())
  {
    DbgPrint("%x:%s<%p>\n", GetCurrentThreadId(), __FUNCTION__, this);
  }
};

ULONG WINAPI MailCli(PVOID name)
{
  if (CMailSlot* p = new CMailSlot)
  {
    if (0 <= p->Open((PCWSTR)name))
    {
      ULONG n = 0, id = GetCurrentThreadId();
      char msg[64];
      WCHAR cap[32];

      swprintf(cap, L"[%x]", id);
      do 
      {
        if (0 > p->Write(msg, sprintf(msg, "-=[%x.%x]=-", id, n++)))
        {
          break;
        }
      } while (MessageBoxW(0, L"continue ?", cap, MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2) == IDYES);

      p->Close();
    }
    
    p->Release();
  }
  return 0;
}

class SMailSlot : public MailSlot
{
  virtual void OnRead(NTSTATUS status, PVOID buf, ULONG dwNumberOfBytesTransfered, CDataPacket* packet)
  {
    if (0 > status)
    {
      return ;
    }

    DbgPrint("[%x:%p]: %.*s\n", GetCurrentThreadId(), packet, dwNumberOfBytesTransfered, buf);

    Read(packet);
  }
  
  ~SMailSlot()
  {
    DbgPrint("%x:%s<%p>\n", GetCurrentThreadId(), __FUNCTION__, this);
  }

public:
  SMailSlot()
  {
    DbgPrint("%x:%s<%p>\n", GetCurrentThreadId(), __FUNCTION__, this);
  }
protected:
private:
};

void MailSrv(PCWSTR name)
{
  if (SMailSlot* p = new SMailSlot)
  {
    if (0 <= p->Create(name))
    {
      ULONG n = 2;
      do 
      {
        if (CDataPacket* packet = new(PAGE_SIZE) CDataPacket)
        {
          p->Read(packet);
          packet->Release();
        }
      } while (--n);

      n = 4;
      do 
      {
        CloseHandle(CreateThread(0, 0, MailCli, (void*)name, 0, 0));
      } while (--n);

      MessageBoxW(0, 0, L"server", MB_ICONINFORMATION);

      p->Close();
    }
    p->Release();
  }

  g_IoRundown->BeginRundown();

  ExitThread(0);
}

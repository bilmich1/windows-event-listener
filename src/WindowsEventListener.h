#pragma once

#include "EventListener.h"

#include <Windows.h>
#include <winevt.h>

class WindowsEventListener : public IEventListener
{
public:
    WindowsEventListener();
    ~WindowsEventListener();

    void start() override;

    DWORD callbackImpl(EVT_SUBSCRIBE_NOTIFY_ACTION action, EVT_HANDLE event_handle);

private:
    void cleanup();

    EVT_HANDLE event_handle_ = nullptr;
};

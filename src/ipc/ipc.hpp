#pragma once
#include "../defines.hpp"

std::string   readFromIPCChannel(int);
int           writeToIPCChannel(int, const std::string&);

#define         IPC_END_OF_FILE (std::string)"HYPR_END_OF_FILE"
#define         IPC_MESSAGE_SEPARATOR std::string("\t")
#define         IPC_MESSAGE_EQUALITY std::string("=")

struct SIPCMessageMainToBar {
    std::vector<int>    openWorkspaces;
    uint64_t            activeWorkspace;
    std::string         lastWindowName;
    std::string         lastWindowClass;
    bool                fullscreenOnBar;
};

struct SIPCMessageBarToMain {
    uint64_t            windowID;
};

struct SIPCPipe {
    int         iPipeFD = -1;
};

// IPC is implemented with an anonymous Unix-domain socketpair shared by the
// parent and bar child processes.

void         IPCSendMessage(int, SIPCMessageMainToBar);
void         IPCSendMessage(int, SIPCMessageBarToMain);
void         IPCRecieveMessageB(int);
void         IPCRecieveMessageM(int);
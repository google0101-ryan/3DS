// (c) Copyright 2022 Ryan Ilari
// This code is licensed under MIT license (see LICENSE for details)

#include "Application.h"
#include <signal.h>
#include <System.h>

bool Application::isRunning = false;
int Application::exit_code = 0;

void Sig(int)
{
    Application::Exit();
}

bool Application::Init(int argc, char** argv)
{
	if (argc < 3)
    {
        printf("Usage: %s [bios9] [bios11]\n", argv[0]);
        return false;
    }

    bool success = false;

    printf("Initializing System\n");

	System::LoadBios(argv[1], argv[2]);
	System::Reset();

    std::atexit(Application::Exit);
    // signal(SIGSEGV, Sig);
    signal(SIGINT, Application::Exit);
    signal(SIGABRT, Application::Exit);
    
    isRunning = true;

    return true;
}

int Application::Run()
{
	return System::Run();
}

void Application::Exit(int code)
{
    exit_code = code;
    isRunning = false;
    exit(1);
}

void Application::Exit()
{
	System::Dump();
}
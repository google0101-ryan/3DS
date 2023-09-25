#include "app/Application.h"

int main(int argc, char** argv)
{
    if (!Application::Init(argc, argv))
    {
        printf("Error initializing main app class\n");
        exit(1);
    }

    return Application::Run();
}
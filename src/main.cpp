
#include "EventListener.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    std::cout << "Windows Event Listener\n\n";

    EventListener event_listener;
    event_listener.registerObserver([](const std::string& event) {std::cout << "\n*** EVENT BEGIN ***\n" << event << "\n*** EVENT END ***\n"; });

    do
    {
        std::cout << '\n' << "Press a key to continue...";
    } while (std::cin.get() != '\n');
}

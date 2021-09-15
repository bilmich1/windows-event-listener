
#include "EventListener.h"

#include <boost/property_tree/xml_parser.hpp>

#include <iostream>
#include <string>

int main()
{
    std::cout << "Windows Event Listener\n\n";

    auto event_listener = makeEventListener();
    event_listener->registerObserver([](const IEventListener::PublishType& event) {
        std::cout << "\n*** EVENT BEGIN ***\n";
        if (std::holds_alternative<boost::property_tree::ptree>(event))
        {
            boost::property_tree::write_xml(std::cout, std::get<boost::property_tree::ptree>(event));
        }
        else
        {
            try
            {
                std::rethrow_exception(std::get<std::exception_ptr>(event));
            }
            catch (const std::exception& ex)
            {
                std::cout << "Error: " << ex.what() << "\n";
            }
        }
        std::cout << "\n*** EVENT END ***\n";
        });

    event_listener->start();

    do
    {
        std::cout << '\n' << "Press a key to continue...";
    } while (std::cin.get() != '\n');

    event_listener->updateBookmark();
}

cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable (windows-event-listener "main.cpp" "EventListener.h" "EventListener.cpp" "WindowsEventListener.h" "WindowsEventListener.cpp")

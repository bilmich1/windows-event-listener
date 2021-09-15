add_executable(${PROJECT_NAME} main.cpp EventListener.h EventListener.cpp WindowsEventListener.h WindowsEventListener.cpp "Bookmark.h" "Bookmark.cpp" "Details.h" "Details.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::boost)

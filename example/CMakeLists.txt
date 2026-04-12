find_package(Qt6 6.4 REQUIRED COMPONENTS
    Widgets
)

add_executable(config_provider_example
    main.cpp
    contact.h contact.cpp
)

target_link_libraries(config_provider_example PRIVATE
    Qt6::Core
    Qt6::Widgets
    mmolch_qtutil_config_provider
)

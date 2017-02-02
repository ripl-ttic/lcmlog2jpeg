add_definitions(-std=gnu99)

add_executable(er-lcmlog2jpeg main.c)

pods_use_pkg_config_packages(er-lcmlog2jpeg 
    glib-2.0
    lcm 
    bot2-core)     

pods_install_executables(er-lcmlog2jpeg)

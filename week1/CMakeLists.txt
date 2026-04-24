file(GLOB DAY_SOURCES "day*.cpp")
foreach(SOURCE ${DAY_SOURCES})
    add_practice_day(${SOURCE})
endforeach()

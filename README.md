height-map-shadow
=================

Fast sweep algorithm for generating a shadow boundary heightmap from a source heightmap.

To test, use the command line,

    g++ -D TEST_HMS HeightMapShadow.cpp targa.c -o hms && ./hms

Or indeed,

    clang++ -D TEST_HMS HeightMapShadow.cpp targa.c -o hms && ./hms


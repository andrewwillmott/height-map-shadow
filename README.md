height-map-shadow
=================

Fast sweep algorithm for generating a shadow boundary heightmap from a source 
heightmap. Originally used (way!) back in the day for Sims 2/SimCity 4 terrain shadowing,
these days occasionally useful for generating height map self-shadowing in the context
of texture processing.

To test, use the command line,

    c++ HeightMapShadow.cpp HeightMapShadowTest.cpp -o hms && ./hms

Or add those files to your favourite IDE.

See http://www.andrewwillmott.com/tech-notes#TOC-Height-Map-Shadows for more 
info.

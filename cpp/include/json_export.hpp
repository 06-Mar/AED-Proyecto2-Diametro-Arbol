#pragma once
#include "diameter.hpp"

void exportJson(const char* outputPath, const Tree& tree, const BFSResult& first,
                const BFSResult& second, const DiameterResult& diameter, const Trace& trace);

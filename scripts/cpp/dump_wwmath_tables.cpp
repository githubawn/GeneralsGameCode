#include <iostream>
#include <fstream>
#include <iomanip>
#include "WWMath/wwmath.h"

// Assuming this script is compiled alongside WWMath or placed into the game's initialization logic.
void DumpWWMathTables()
{
    // Ensure WWMath is initialized first, which populates the tables based on the host's sin/cos functions.
    // WWMath::Init(); 

    std::ofstream out("wwmath_legacy_tables.h");
    
    out << "// Auto-generated backward compatibility tables for WWMath\n";
    out << "// These values were captured from the original x86 client.\n\n";
    out << "#pragma once\n\n";

    out << "const float legacy_FastSinTable[" << SIN_TABLE_SIZE << "] = {\n    ";
    for(int i = 0; i < SIN_TABLE_SIZE; ++i)
    {
        // Print with 9 decimal digits of precision to capture exactly 32 bits of float
        out << std::fixed << std::setprecision(9) << _FastSinTable[i] << "f";
        if (i < SIN_TABLE_SIZE - 1) out << ", ";
        if ((i + 1) % 8 == 0) out << "\n    ";
    }
    out << "\n};\n\n";

    out << "const float legacy_FastAcosTable[" << ARC_TABLE_SIZE << "] = {\n    ";
    for(int i = 0; i < ARC_TABLE_SIZE; ++i)
    {
        out << std::fixed << std::setprecision(9) << _FastAcosTable[i] << "f";
        if (i < ARC_TABLE_SIZE - 1) out << ", ";
        if ((i + 1) % 8 == 0) out << "\n    ";
    }
    out << "\n};\n\n";

    out << "const float legacy_FastAsinTable[" << ARC_TABLE_SIZE << "] = {\n    ";
    for(int i = 0; i < ARC_TABLE_SIZE; ++i)
    {
        out << std::fixed << std::setprecision(9) << _FastAsinTable[i] << "f";
        if (i < ARC_TABLE_SIZE - 1) out << ", ";
        if ((i + 1) % 8 == 0) out << "\n    ";
    }
    out << "\n};\n\n";

    out.close();
    std::cout << "Successfully dumped WWMath tables to wwmath_legacy_tables.h.\n";
}

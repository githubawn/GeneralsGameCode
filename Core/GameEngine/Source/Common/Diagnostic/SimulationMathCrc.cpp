/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "PreRTS.h"

#include "Common/Diagnostic/SimulationMathCrc.h"
#include "Common/XferCRC.h"
#include "WWMath/matrix3d.h"
#include "WWMath/wwmath.h"
#include "GameLogic/FPUControl.h"

#include "WWMath/DeterministicMath.h"
#include <stdio.h>

static void appendSimulationMathCrc(XferCRC &xfer)
{
    Matrix3D matrix;
    Matrix3D factorsMatrix;

    matrix.Set(
        4.1f, 1.2f, 0.3f, 0.4f,
        0.5f, 3.6f, 0.7f, 0.8f,
        0.9f, 1.0f, 2.1f, 1.2f);

    factorsMatrix.Set(
        DeterministicMath::Sin(0.7f) * DeterministicMath::Log10(2.3f),
        DeterministicMath::Cos(1.1f) * DeterministicMath::Pow(1.1f, 2.0f),
        DeterministicMath::Tan(0.3f),
        DeterministicMath::ASin(0.967302263f),
        DeterministicMath::ACos(0.967302263f),
        DeterministicMath::ATan(0.967302263f) * DeterministicMath::Pow(1.1f, 2.0f),
        DeterministicMath::ATan2(0.4f, 1.3f),
        DeterministicMath::Sinh(0.2f),
        DeterministicMath::Cosh(0.4f) * DeterministicMath::Tanh(0.5f),
        DeterministicMath::Sqrt(55788.84375f),
        DeterministicMath::Exp(0.1f) * DeterministicMath::Log10(2.3f),
        DeterministicMath::Log(1.4f));

    Matrix3D::Multiply(matrix, factorsMatrix, &matrix);
    matrix.Get_Inverse(matrix);

    xfer.xferMatrix3D(&matrix);
}

static UnsignedInt calculateWWMathTableCRC()
{
    XferCRC xfer;
    xfer.open("WWMathTables");
    
    xfer.xferUser((void*)_FastSinTable, sizeof(_FastSinTable));
    xfer.xferUser((void*)_FastAcosTable, sizeof(_FastAcosTable));
    xfer.xferUser((void*)_FastAsinTable, sizeof(_FastAsinTable));
    
    xfer.close();
    return xfer.getCRC();
}

UnsignedInt SimulationMathCrc::calculate()
{
    XferCRC xfer;
    xfer.open("SimulationMathCrc");

    appendSimulationMathCrc(xfer);

    xfer.close();
    UnsignedInt crc = xfer.getCRC();

    // TheSuperHackers @diagnostic Output to file for retail builds without debug logs
    FILE *f = fopen("SimulationMathCrc.txt", "w");
    if (f)
    {
        fprintf(f, "SimulationMathCrc: 0x%08X\n", crc);
        fprintf(f, "WWMathTablesCrc:   0x%08X\n", calculateWWMathTableCRC());
        fclose(f);
    }

    return crc;
}

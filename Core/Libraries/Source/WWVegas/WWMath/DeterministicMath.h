#pragma once

#include "Lib/BaseType.h"

/**
 * DeterministicMath provides bit-exact floating point operations across different CPU architectures.
 * These implementations avoid platform-specific hardware instructions (like x87 fsin) and 
 * instead use portable software approximations.
 */
namespace DeterministicMath
{
    float Sin(float x);
    float Cos(float x);
    float Tan(float x);
    float ASin(float x);
    float ACos(float x);
    float ATan(float x);
    float ATan2(float y, float x);
    float Pow(float base, float exp);
    float Sqrt(float x);
    float Exp(float x);
    float Log(float x);
    float Log10(float x);
    float Sinh(float x);
    float Cosh(float x);
    float Tanh(float x);
}

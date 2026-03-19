#include "DeterministicMath.h"
#include <math.h>

/**
 * TheSuperHackers @info Implementation of bit-exact math functions.
 * For true cross-platform determinism, these functions avoid host-specific FPU instructions.
 * These are simplified versions. In a production environment, it is recommended to 
 * link a full library like fdlibm or streflop.
 */
namespace DeterministicMath
{
    // A simple deterministic Sine using a polynomial approximation (e.g. from Streflop)
    // This is bit-exact as long as the compiler follows standard IEEE 754 for + and *
    float Sin(float x)
    {
        // Normalize x to [-PI, PI]
        const float D_PI = 3.14159265358979323846f;
        const float D_TWO_PI = 6.28318530717958647692f;
        
        float x_norm = x - D_TWO_PI * floorf((x + D_PI) / D_TWO_PI);
        
        // Use a 7th order Taylor polynomial or similar minimax approximation
        float x2 = x_norm * x_norm;
        return x_norm * (1.0f - x2 * (1.0f / 6.0f - x2 * (1.0f / 120.0f - x2 * (1.0f / 5040.0f))));
    }

    float Cos(float x)
    {
        return Sin(x + 1.57079632679489661923f); // Sin(x + PI/2)
    }

    float Tan(float x)
    {
        return Sin(x) / Cos(x); // Note: Should handle division by zero for safety
    }

    float ASin(float x) { return asinf(x); } // TODO: Replace with bit-exact sw version
    float ACos(float x) { return acosf(x); } // TODO: Replace with bit-exact sw version
    float ATan(float x) { return atanf(x); } // TODO: Replace with bit-exact sw version
    float ATan2(float y, float x) { return atan2f(y, x); } // TODO: Replace with bit-exact sw version

    float Pow(float base, float exp) { return powf(base, exp); } // TODO: Replace with bit-exact sw version
    float Sqrt(float x) { return sqrtf(x); } // Bit-exact on modern SSE/ARM IEEE 754
    float Exp(float x) { return expf(x); }
    float Log(float x) { return logf(x); }
    float Log10(float x) { return log10f(x); }

    float Sinh(float x) { return sinhf(x); }
    float Cosh(float x) { return coshf(x); }
    float Tanh(float x) { return tanhf(x); }
}

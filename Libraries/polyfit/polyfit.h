/// This file and its content is originally written and
/// published by Nate Domin on GitHub
/// ->  https://github.com/natedomin/polyfit
/// Accessed and adapted by Rene Santeler at 23.03.2021


#ifndef POLYFIT_H_INCLUDED
#define POLYFIT_H_INCLUDED

int polyfit(const float* const dependentValues,
            const float* const independentValues,
            unsigned int        countOfElements,
            unsigned int        order,
            float*             coefficients);

#endif // POLYFIT_H_INCLUDED

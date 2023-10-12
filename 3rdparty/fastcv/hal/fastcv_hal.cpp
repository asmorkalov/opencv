#include "fastcv_hal.hpp"
#include "fastcv.h"
#include <cstdint>
#include <stdio.h>

static const char* getFastCVErrorString(fcvStatus status)
{
    switch(status)
    {
        case FASTCV_SUCCESS: return "Succesful";
        case FASTCV_EFAIL: return "General failure";
        case FASTCV_EUNALIGNPARAM: return "Unaligned pointer parameter";
        case FASTCV_EBADPARAM: return "Bad parameters";
        case FASTCV_EINVALSTATE: return "Called at invalid state";
        case FASTCV_ENORES: return "Insufficient resources, memory, thread, etc";
        case FASTCV_EUNSUPPORTED: return "Unsupported feature";
        case FASTCV_EHWQDSP: return "Hardware QDSP failed to respond";
        case FASTCV_EHWGPU: return "Hardware GPU failed to respond";
    }
}

int fastcv_hal_add_8u(const uchar *a, size_t astep, const uchar *b, size_t bstep, uchar *c, size_t cstep, int w, int h)
{
    printf("width: %d, height: %d\n", w, h);
    printf("astep: %zu, bstep: %zu, cstep: %zu\n", astep, bstep, cstep);

    // stride shpuld be miltiple of 8
    if ((astep % 8 != 0) || (bstep % 8 != 0) || (cstep % 8 != 0))
        return CV_HAL_ERROR_NOT_IMPLEMENTED;

    // 128-bit alignment check
    if (((uintptr_t)a % 16) || ((uintptr_t)b % 16) || ((uintptr_t)c % 16))
        return CV_HAL_ERROR_NOT_IMPLEMENTED;

    fcvStatus status = fcvAddu8(a, w, h, astep, b, bstep, FASTCV_CONVERT_POLICY_SATURATE, c, cstep);
    if (status == FASTCV_SUCCESS)
        return CV_HAL_ERROR_OK;
    else
    {
        printf("FastCV error: %s\n", getFastCVErrorString(status));
        return CV_HAL_ERROR_UNKNOWN;
    }
}

#ifndef OPENCV_FASTCV_HAL_HPP_INCLUDED
#define OPENCV_FASTCV_HAL_HPP_INCLUDED

#include "opencv2/core/hal/interface.h"

int fastcv_hal_add_8u(const uchar *a, size_t astep, const uchar *b, size_t bstep, uchar *c, size_t cstep, int w, int h);

#undef cv_hal_add8u
#define cv_hal_add8u fastcv_hal_add_8u

#endif

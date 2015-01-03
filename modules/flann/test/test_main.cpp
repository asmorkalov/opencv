#include "test_precomp.hpp"


// You shound not call it. It's added to fix linkage problem with CUDA 7.0 aarch64

extern "C"
{

    void nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R_NEW_SM30()
    {}

    void nppiDCTQuantInv8x8LS_JPEG_16s8u_C1R_NEW_SM30()
    {}

}

CV_TEST_MAIN("cv")

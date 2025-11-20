// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#include "test_precomp.hpp"
#include "opencv2/ts/ocl_test.hpp"

namespace opencv_test {
namespace ocl {

static
testing::internal::ParamGenerator<std::string> getOpenCLTestConfigurations()
{
    if (!cv::ocl::useOpenCL())
    {
        return testing::ValuesIn(std::vector<std::string>());
    }

    std::vector<std::string> configurations = {
        ":GPU:0",
        ":GPU:1",
        ":CPU:0",
    };
    return testing::ValuesIn(configurations);
}


static void executeUMatCall(bool requireOpenCL = true)
{
    UMat a(100, 100, CV_8UC1, Scalar::all(0));
    UMat b;
    cv::add(a, Scalar::all(1), b);
    Mat b_cpu = b.getMat(ACCESS_READ);
    EXPECT_EQ(0, cv::norm(b_cpu - 1, NORM_INF));

    if (requireOpenCL)
    {
        EXPECT_TRUE(cv::ocl::useOpenCL());
    }
}

TEST(OCL_Context, createFromDevice)
{
    bool useOCL = cv::ocl::useOpenCL();

    OpenCLExecutionContext ctx = OpenCLExecutionContext::getCurrent();

    if (!useOCL)
    {
        ASSERT_TRUE(ctx.empty());  // Other tests should not broke global state
        throw SkipTestException("OpenCL is not available / disabled");
    }

    ASSERT_FALSE(ctx.empty());

    ocl::Device device = ctx.getDevice();
    ASSERT_FALSE(device.empty());

    ocl::Context context = ocl::Context::fromDevice(device);
    ocl::Context context2 = ocl::Context::fromDevice(device);

    EXPECT_TRUE(context.getImpl() == context2.getImpl()) << "Broken cache for OpenCL context (device)";
}

TEST(OCL_OpenCLExecutionContextDefault, basic)
{
    bool useOCL = cv::ocl::useOpenCL();

    OpenCLExecutionContext ctx = OpenCLExecutionContext::getCurrent();

    if (!useOCL)
    {
        ASSERT_TRUE(ctx.empty());  // Other tests should not broke global state
        throw SkipTestException("OpenCL is not available / disabled");
    }

    ASSERT_FALSE(ctx.empty());

    ocl::Context context = ctx.getContext();
    ocl::Context context2 = ocl::Context::getDefault();
    EXPECT_TRUE(context.getImpl() == context2.getImpl());

    ocl::Device device = ctx.getDevice();
    ocl::Device device2 = ocl::Device::getDefault();
    EXPECT_TRUE(device.getImpl() == device2.getImpl());

    ocl::Queue queue = ctx.getQueue();
    ocl::Queue queue2 = ocl::Queue::getDefault();
    EXPECT_TRUE(queue.getImpl() == queue2.getImpl());
}

TEST(OCL_OpenCLExecutionContextDefault, createAndBind)
{
    bool useOCL = cv::ocl::useOpenCL();

    OpenCLExecutionContext ctx = OpenCLExecutionContext::getCurrent();

    if (!useOCL)
    {
        ASSERT_TRUE(ctx.empty());  // Other tests should not broke global state
        throw SkipTestException("OpenCL is not available / disabled");
    }

    ASSERT_FALSE(ctx.empty());

    ocl::Context context = ctx.getContext();
    ocl::Device device = ctx.getDevice();

    OpenCLExecutionContext ctx2 = OpenCLExecutionContext::create(context, device);
    ASSERT_FALSE(ctx2.empty());

    try
    {
        ctx2.bind();
        executeUMatCall();
        ctx.bind();
        executeUMatCall();
    }
    catch (...)
    {
        ctx.bind();  // restore
        throw;
    }
}

typedef testing::TestWithParam<std::string> OCL_OpenCLExecutionContext_P;

TEST_P(OCL_OpenCLExecutionContext_P, multipleBindAndExecute)
{
    bool useOCL = cv::ocl::useOpenCL();

    OpenCLExecutionContext ctx = OpenCLExecutionContext::getCurrent();

    if (!useOCL)
    {
        ASSERT_TRUE(ctx.empty());  // Other tests should not broke global state
        throw SkipTestException("OpenCL is not available / disabled");
    }

    ASSERT_FALSE(ctx.empty());

    std::string opencl_device = GetParam();
    ocl::Context context = ocl::Context::create(opencl_device);
    if (context.empty())
    {
        throw SkipTestException(std::string("OpenCL device is not available: '") + opencl_device + "'");
    }

    ocl::Device device = context.device(0);

    OpenCLExecutionContext ctx2 = OpenCLExecutionContext::create(context, device);
    ASSERT_FALSE(ctx2.empty());

    try
    {
        std::cout << "ctx2..." << std::endl;
        ctx2.bind();
        executeUMatCall();
        std::cout << "ctx..." << std::endl;
        ctx.bind();
        executeUMatCall();
    }
    catch (...)
    {
        ctx.bind();  // restore
        throw;
    }
}

TEST_P(OCL_OpenCLExecutionContext_P, ScopeTest)
{
    bool useOCL = cv::ocl::useOpenCL();

    OpenCLExecutionContext ctx = OpenCLExecutionContext::getCurrent();

    if (!useOCL)
    {
        ASSERT_TRUE(ctx.empty());  // Other tests should not broke global state
        throw SkipTestException("OpenCL is not available / disabled");
    }

    ASSERT_FALSE(ctx.empty());

    std::string opencl_device = GetParam();
    ocl::Context context = ocl::Context::create(opencl_device);
    if (context.empty())
    {
        throw SkipTestException(std::string("OpenCL device is not available: '") + opencl_device + "'");
    }

    ocl::Device device = context.device(0);

    OpenCLExecutionContext ctx2 = OpenCLExecutionContext::create(context, device);
    ASSERT_FALSE(ctx2.empty());

    try
    {
        OpenCLExecutionContextScope ctx_scope(ctx2);
        executeUMatCall();
    }
    catch (...)
    {
        ctx.bind();  // restore
        throw;
    }

    executeUMatCall();
}

INSTANTIATE_TEST_CASE_P(/*nothing*/, OCL_OpenCLExecutionContext_P, getOpenCLTestConfigurations());


typedef testing::TestWithParam<UMatUsageFlags> UsageFlagsFixture;
OCL_TEST_P(UsageFlagsFixture, UsageFlagsRetained)
{
    if (!cv::ocl::useOpenCL())
    {
        throw SkipTestException("OpenCL is not available / disabled");
    }

    const UMatUsageFlags usage = GetParam();
    cv::UMat flip_in(10, 10, CV_32F, usage);
    cv::UMat flip_out(usage);
    cv::flip(flip_in, flip_out, 1);
    cv::ocl::finish();

    ASSERT_EQ(usage, flip_in.usageFlags);
    ASSERT_EQ(usage, flip_out.usageFlags);
}

INSTANTIATE_TEST_CASE_P(
    /*nothing*/,
    UsageFlagsFixture,
    testing::Values(USAGE_DEFAULT, USAGE_ALLOCATE_HOST_MEMORY, USAGE_ALLOCATE_DEVICE_MEMORY)
);

TEST(OpenCLMultiContext, UMatDtors)
{
    bool useOCL = cv::ocl::useOpenCL();

    if (!useOCL)
    {
        throw SkipTestException("OpenCL is not available / disabled");
    }

    cv::ocl::OpenCLExecutionContext current = cv::ocl::OpenCLExecutionContext::getCurrent();

    std::vector<cv::ocl::PlatformInfo> platform_info;
    cv::ocl::getPlatfomsInfo(platform_info);
    std::vector<cv::ocl::Device> devices;

    for (size_t p = 0; p <platform_info.size(); p++)
    {
        printf("Platform: %s\n", platform_info[p].name().c_str());
        int dc = platform_info[p].deviceNumber();
        for (int d = 0; d < dc; d++)
        {
            cv::ocl::Device device;
            platform_info[p].getDevice(device, d);
            printf("\tDevice: %s\n", device.name().c_str());
            devices.push_back(device);
        }
    }

    printf("Found %d OpenCL devices\n", (int)devices.size());
    if (devices.size() < 2)
    {
        throw SkipTestException("Not enough OpenCL devices for context interop test");
    }

    std::vector<cv::ocl::OpenCLExecutionContext> contexts(devices.size());
    std::vector<cv::UMat> buffers(devices.size());

    for (int i = 0; i < static_cast<int>(devices.size()); i++)
    {
        printf("Device %d\n", i);
        cv::ocl::Context ctx = cv::ocl::Context::fromDevice(devices[i]);
        printf("\tContext created\n");
        contexts[i] = cv::ocl::OpenCLExecutionContext::create(ctx, devices[i]);
        printf("\tExecution Context created\n");
        contexts[i].bind();
        printf("\tExecution Context binded\n");
        buffers[i] = cv::UMat(1024, 1, CV_8UC1, cv::Scalar(255));
        printf("\tBuffer created\n");
    }

    // The buffers are destroyed in reverse order.
    // The buffer should be desctroyed with the context it was created, but not the current one
    for (int i = static_cast<int>(devices.size()); i >= 0; i++)
    {
        buffers[i].release();
    }

    buffers.clear();
    contexts.clear();

    current.bind();
}

} } // namespace opencv_test::ocl

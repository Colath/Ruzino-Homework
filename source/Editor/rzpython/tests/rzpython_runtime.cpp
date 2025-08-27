#include <gtest/gtest.h>

#include <RHI/rhi.hpp>

#include "GUI/window.h"
#include "rzpython/rzpython.hpp"

using namespace USTC_CG;

TEST(RZPythonRuntimeTest, RHI_package)
{
    python::initialize();

    python::import("RHI_py");
    int result = python::call<int>("RHI_py.init()");
    EXPECT_EQ(result, 0);

    // Test that we can call get_device without crashing, even if we can't convert the result yet
    python::call<void>("device = RHI_py.get_device()");
    python::call<void>("print('Device type:', type(device))");
    
    // Test that we can get the backend enum
    python::call<void>("backend = RHI_py.get_backend()");
    python::call<void>("print('Backend type:', type(backend))");
    python::call<void>("print('Backend value:', backend)");

    result = python::call<int>("RHI_py.shutdown()");
    EXPECT_EQ(result, 0);

    python::finalize();
}

TEST(RZPythonRuntimeTest, GUI_package)
{
    python::initialize();

    python::import("GUI_py");

    Window window;
    python::reference("w", &window);
    
    // Just test that we can call the method without crashing
    // and that we get some kind of numeric result
    python::call<void>("print('Testing Window binding...')");
    python::call<void>("print(type(w))");
    python::call<void>("result = w.get_elapsed_time()");
    python::call<void>("print('Elapsed time result:', result)");

    float time = python::call<float>("w.get_elapsed_time()");
    // Just check that we get a finite number (not inf or nan)
    EXPECT_TRUE(std::isfinite(time));

    python::finalize();
}

#include <GUI/window.h>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

#include <rzpython/rzpython.hpp>
#include <stdexcept>
#include <unordered_map>

namespace nb = nanobind;

USTC_CG_NAMESPACE_OPEN_SCOPE

namespace python {

// Global variables - accessible from template implementations
PyObject* main_module = nullptr;
PyObject* main_dict = nullptr;
bool initialized = false;
std::unordered_map<std::string, nb::object> bound_objects;

void initialize()
{
    if (initialized) {
        return;
    }

    // Add path to ensure Python finds our modules
    Py_Initialize();
    if (!Py_IsInitialized()) {
        throw std::runtime_error("Failed to initialize Python interpreter");
    }

    // Simple initialization marker without problematic import hooks
    try {
        PyRun_SimpleString(
            "import sys\n"
            "sys._rzpython_initialized = True\n");
    }
    catch (...) {
        // Ignore setup errors
    }

    main_module = PyImport_AddModule("__main__");
    if (!main_module) {
        throw std::runtime_error("Failed to get __main__ module");
    }

    main_dict = PyModule_GetDict(main_module);
    if (!main_dict) {
        throw std::runtime_error("Failed to get __main__ dictionary");
    }

    initialized = true;
}

void finalize()
{
    if (!initialized) {
        return;
    }

    // Clear our bound objects
    try {
        for (const auto& pair : bound_objects) {
            PyDict_DelItemString(main_dict, pair.first.c_str());
        }
        bound_objects.clear();
    }
    catch (...) {
        // Ignore cleanup errors
    }

    // Reset main module references
    main_module = nullptr;
    main_dict = nullptr;

    // Finalize Python interpreter
    Py_Finalize();

    initialized = false;
}

void import(const std::string& module_name)
{
    if (!initialized) {
        throw std::runtime_error("Python interpreter not initialized");
    }

    PyObject* module = PyImport_ImportModule(module_name.c_str());
    if (!module) {
        PyErr_Print();
        throw std::runtime_error("Failed to import module: " + module_name);
    }

    // Add module to main dict so it can be accessed
    PyDict_SetItemString(main_dict, module_name.c_str(), module);
    Py_DECREF(module);
}

// Template specializations for call function
template<>
int call<int>(const std::string& code)
{
    if (!initialized) {
        throw std::runtime_error("Python interpreter not initialized");
    }

    PyObject* result =
        PyRun_String(code.c_str(), Py_eval_input, main_dict, main_dict);
    if (!result) {
        PyErr_Print();
        throw std::runtime_error("Failed to execute Python code: " + code);
    }

    if (!PyLong_Check(result)) {
        Py_DECREF(result);
        throw std::runtime_error("Expected int return type");
    }

    int value = PyLong_AsLong(result);
    Py_DECREF(result);
    return value;
}

template<>
float call<float>(const std::string& code)
{
    if (!initialized) {
        throw std::runtime_error("Python interpreter not initialized");
    }

    PyObject* result =
        PyRun_String(code.c_str(), Py_eval_input, main_dict, main_dict);
    if (!result) {
        PyErr_Print();
        throw std::runtime_error("Failed to execute Python code: " + code);
    }

    if (!PyFloat_Check(result) && !PyLong_Check(result)) {
        Py_DECREF(result);
        throw std::runtime_error("Expected float return type");
    }

    float value = PyFloat_AsDouble(result);
    Py_DECREF(result);
    return value;
}

template<>
void call<void>(const std::string& code)
{
    if (!initialized) {
        throw std::runtime_error("Python interpreter not initialized");
    }

    PyObject* result =
        PyRun_String(code.c_str(), Py_file_input, main_dict, main_dict);
    if (!result) {
        PyErr_Print();
        throw std::runtime_error("Failed to execute Python code: " + code);
    }

    Py_DECREF(result);
}

// Internal helper for raw Python object return
PyObject* call_raw(const std::string& code)
{
    if (!initialized) {
        throw std::runtime_error("Python interpreter not initialized");
    }

    PyObject* result =
        PyRun_String(code.c_str(), Py_eval_input, main_dict, main_dict);
    if (!result) {
        PyErr_Print();
        throw std::runtime_error("Failed to execute Python code: " + code);
    }

    return result;  // Caller is responsible for DECREF
}

// Template specializations for std::vector types
template<>
std::vector<int> call<std::vector<int>>(const std::string& code)
{
    PyObject* py_result = call_raw(code);
    if (!py_result) {
        throw std::runtime_error(
            "Failed to get result from Python code: " + code);
    }

    try {
        // Use nanobind to convert the Python object to std::vector<int>
        nb::object nb_result = nb::steal(py_result);  // Takes ownership
        std::vector<int> result = nb::cast<std::vector<int>>(nb_result);
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            "Failed to convert Python result to std::vector<int>: " +
            std::string(e.what()));
    }
}

template<>
std::vector<float> call<std::vector<float>>(const std::string& code)
{
    PyObject* py_result = call_raw(code);
    if (!py_result) {
        throw std::runtime_error(
            "Failed to get result from Python code: " + code);
    }

    try {
        // Use nanobind to convert the Python object to std::vector<float>
        nb::object nb_result = nb::steal(py_result);  // Takes ownership
        std::vector<float> result = nb::cast<std::vector<float>>(nb_result);
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            "Failed to convert Python result to std::vector<float>: " +
            std::string(e.what()));
    }
}

template<>
std::vector<std::string> call<std::vector<std::string>>(const std::string& code)
{
    PyObject* py_result = call_raw(code);
    if (!py_result) {
        throw std::runtime_error(
            "Failed to get result from Python code: " + code);
    }

    try {
        // Use nanobind to convert the Python object to std::vector<std::string>
        nb::object nb_result = nb::steal(py_result);  // Takes ownership
        std::vector<std::string> result =
            nb::cast<std::vector<std::string>>(nb_result);
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            "Failed to convert Python result to std::vector<std::string>: " +
            std::string(e.what()));
    }
}

// Template specializations for ndarray types
template<>
numpy_array_f32 call<numpy_array_f32>(const std::string& code)
{
    PyObject* py_result = call_raw(code);
    if (!py_result) {
        throw std::runtime_error(
            "Failed to get result from Python code: " + code);
    }

    try {
        nb::object nb_result = nb::steal(py_result);
        numpy_array_f32 result = nb::cast<numpy_array_f32>(nb_result);
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            "Failed to convert Python result to numpy_array_f32: " +
            std::string(e.what()));
    }
}

template<>
numpy_array_f64 call<numpy_array_f64>(const std::string& code)
{
    PyObject* py_result = call_raw(code);
    if (!py_result) {
        throw std::runtime_error(
            "Failed to get result from Python code: " + code);
    }

    try {
        nb::object nb_result = nb::steal(py_result);
        numpy_array_f64 result = nb::cast<numpy_array_f64>(nb_result);
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            "Failed to convert Python result to numpy_array_f64: " +
            std::string(e.what()));
    }
}

template<>
torch_tensor_f32 call<torch_tensor_f32>(const std::string& code)
{
    PyObject* py_result = call_raw(code);
    if (!py_result) {
        throw std::runtime_error(
            "Failed to get result from Python code: " + code);
    }

    try {
        nb::object nb_result = nb::steal(py_result);
        torch_tensor_f32 result = nb::cast<torch_tensor_f32>(nb_result);
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            "Failed to convert Python result to torch_tensor_f32: " +
            std::string(e.what()));
    }
}

template<>
torch_tensor_f64 call<torch_tensor_f64>(const std::string& code)
{
    PyObject* py_result = call_raw(code);
    if (!py_result) {
        throw std::runtime_error(
            "Failed to get result from Python code: " + code);
    }

    try {
        nb::object nb_result = nb::steal(py_result);
        torch_tensor_f64 result = nb::cast<torch_tensor_f64>(nb_result);
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            "Failed to convert Python result to torch_tensor_f64: " +
            std::string(e.what()));
    }
}

template<>
cuda_array_f32 call<cuda_array_f32>(const std::string& code)
{
    PyObject* py_result = call_raw(code);
    if (!py_result) {
        throw std::runtime_error(
            "Failed to get result from Python code: " + code);
    }

    try {
        nb::object nb_result = nb::steal(py_result);
        cuda_array_f32 result = nb::cast<cuda_array_f32>(nb_result);
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            "Failed to convert Python result to cuda_array_f32: " +
            std::string(e.what()));
    }
}

template<>
cuda_array_f64 call<cuda_array_f64>(const std::string& code)
{
    PyObject* py_result = call_raw(code);
    if (!py_result) {
        throw std::runtime_error(
            "Failed to get result from Python code: " + code);
    }

    try {
        nb::object nb_result = nb::steal(py_result);
        cuda_array_f64 result = nb::cast<cuda_array_f64>(nb_result);
        return result;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            "Failed to convert Python result to cuda_array_f64: " +
            std::string(e.what()));
    }
}

}  // namespace python

USTC_CG_NAMESPACE_CLOSE_SCOPE

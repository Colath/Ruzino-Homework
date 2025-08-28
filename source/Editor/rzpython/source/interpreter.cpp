#include <rzconsole/ConsoleObjects.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <rzpython/interpreter.hpp>
#include <rzpython/rzpython.hpp>

USTC_CG_NAMESPACE_OPEN_SCOPE

namespace python {

PythonInterpreter::PythonInterpreter() : python_initialized_(false)
{
    try {
        python::initialize();
        python_initialized_ = true;

        // Register Python-specific commands
        console::CommandDesc python_cmd = {
            "python",
            "Execute Python code interactively",
            [this](console::Command::Args const& args)
                -> console::Command::Result {
                if (args.size() < 2) {
                    return { false, "Usage: python <code>\n" };
                }

                std::string code;
                for (size_t i = 1; i < args.size(); ++i) {
                    if (i > 1)
                        code += " ";
                    code += args[i];
                }

                auto result = ExecutePythonCode(code);
                return { result.status, result.output };
            }
        };
        console::RegisterCommand(python_cmd);

        console::CommandDesc pyexec_cmd = {
            "exec",
            "Execute Python file",
            [this](console::Command::Args const& args)
                -> console::Command::Result {
                if (args.size() != 2) {
                    return { false, "Usage: exec <filename>\n" };
                }

                std::string code = "exec(open('" + args[1] + "').read())";
                auto result = ExecutePythonCode(code);
                return { result.status, result.output };
            }
        };
        console::RegisterCommand(pyexec_cmd);
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to initialize Python interpreter: {}", e.what());
    }
}

PythonInterpreter::~PythonInterpreter()
{
    if (python_initialized_) {
        try {
            python::finalize();
        }
        catch (...) {
            // Ignore cleanup errors
        }
    }
}

bool PythonInterpreter::ShouldHandleCommand(std::string_view command) const
{
    return python_initialized_ && IsPythonCode(command);
}

PythonInterpreter::Result PythonInterpreter::HandleDirectExecution(
    std::string_view cmdline)
{
    if (!python_initialized_) {
        return { false, "Python interpreter not initialized" };
    }

    return ExecutePythonCode(cmdline);
}

PythonInterpreter::Result PythonInterpreter::Execute(
    std::string_view const cmdline)
{
    // First try Python execution for Python-like code
    if (ShouldHandleCommand(cmdline)) {
        return HandleDirectExecution(cmdline);
    }

    // Fall back to base interpreter for console commands
    return console::Interpreter::Execute(cmdline);
}

std::vector<std::string> PythonInterpreter::Suggest(
    std::string_view const cmdline,
    size_t cursor_pos)
{
    if (python_initialized_ && IsPythonCode(cmdline)) {
        return SuggestPythonCompletion(cmdline);
    }

    return console::Interpreter::Suggest(cmdline, cursor_pos);
}

PythonInterpreter::Result PythonInterpreter::ExecuteCommand(
    std::string_view command,
    const std::vector<std::string>& args)
{
    // Handle Python-specific commands
    if (command == "python" || command == "exec") {
        // These are handled by registered console commands
        return { false, "Command should be handled by console system" };
    }

    return console::Interpreter::ExecuteCommand(command, args);
}

std::vector<std::string> PythonInterpreter::SuggestCommand(
    std::string_view command,
    std::string_view cmdline,
    size_t cursor_pos)
{
    if (command == "python") {
        return SuggestPythonCompletion(cmdline);
    }

    return console::Interpreter::SuggestCommand(command, cmdline, cursor_pos);
}

bool PythonInterpreter::IsValidCommand(std::string_view command) const
{
    return command == "python" || command == "exec" ||
           console::Interpreter::IsValidCommand(command);
}

bool PythonInterpreter::IsPythonCode(std::string_view code) const
{
    if (code.empty())
        return false;

    // Simple heuristics to detect Python code
    // Look for Python keywords or syntax
    static const std::vector<std::string> python_keywords = {
        "import", "from",  "def",   "class", "if",     "else",
        "elif",   "for",   "while", "try",   "except", "finally",
        "with",   "print", "len",   "range", "lambda"
    };

    // Check for assignment operator
    if (code.find('=') != std::string_view::npos &&
        code.find("==") == std::string_view::npos) {
        return true;
    }

    // Check for function calls with parentheses
    if (code.find('(') != std::string_view::npos &&
        code.find(')') != std::string_view::npos) {
        return true;
    }

    // Check for Python keywords
    for (const auto& keyword : python_keywords) {
        if (code.find(keyword) != std::string_view::npos) {
            return true;
        }
    }

    return false;
}

PythonInterpreter::Result PythonInterpreter::ExecutePythonCode(
    std::string_view code)
{
    if (!python_initialized_) {
        return { false, "Python interpreter not initialized" };
    }

    try {
        // Try to execute as expression first (for immediate results)
        try {
            std::string result_code = "str(" + std::string(code) + ")";
            std::string result = python::call<std::string>(result_code);
            return { true, result + "\n" };
        }
        catch (...) {
            // If expression fails, try as statement
            python::call<void>(std::string(code));
            return { true, "" };
        }
    }
    catch (const std::exception& e) {
        return { false, std::string("Python error: ") + e.what() + "\n" };
    }
}

std::vector<std::string> PythonInterpreter::SuggestPythonCompletion(
    std::string_view code)
{
    if (!python_initialized_) {
        return {};
    }

    try {
        // Simple completion - get available names in global scope
        auto globals =
            python::call<std::vector<std::string>>("list(globals().keys())");

        // Filter based on current input
        std::vector<std::string> suggestions;
        std::string prefix;

        // Extract the last word as prefix
        auto pos = code.find_last_of(" \t\n()[]{}.,");
        if (pos != std::string_view::npos) {
            prefix = code.substr(pos + 1);
        }
        else {
            prefix = code;
        }

        for (const auto& name : globals) {
            if (name.size() >= prefix.size() &&
                name.substr(0, prefix.size()) == prefix) {
                suggestions.push_back(name);
            }
        }

        return suggestions;
    }
    catch (...) {
        return {};
    }
}

std::shared_ptr<console::Interpreter> CreatePythonInterpreter()
{
    return std::make_shared<PythonInterpreter>();
}

}  // namespace python

USTC_CG_NAMESPACE_CLOSE_SCOPE

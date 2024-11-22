//
// Copyright 2024 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#include "pxr/pxr.h"

#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/pxrCLI11/CLI11.h"

#include "pxr/base/arch/fileSystem.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

#ifdef PXR_PYTHON_SUPPORT_ENABLED
#include "pxr/base/tf/pyInvoke.h"
#endif

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_CLI;

#ifdef PXR_PYTHON_SUPPORT_ENABLED
using namespace pxr_boost::python;
#endif

struct Args {
    std::string inputFile;
    bool skipVariants = false;
    bool rootPackageOnly = false;
    std::string outFile = "stdout";
    bool noAssetChecks = false;
    bool arkit = false;
    bool dumpRules = false;
    bool verbose = false;
    bool strict = false;
};

static
void Configure(CLI::App* app, Args& args) {
    app->add_option(
        "inputFile", args.inputFile, 
        "Name of the input file to inspect.")
        ->type_name("FILE");
    app->add_flag(
        "-s, --skipVariants", args.skipVariants, 
        "If specified, only the prims that are present in the default (i.e. "
        "selected) variants are checked. When this option is not specified, "
        "prims in all possible combinations of variant selections are "
        "checked.");
    app->add_flag(
        "-p, --rootPackageOnly", args.rootPackageOnly, 
        "Check only the specified package. Nested packages, dependencies and "
        "their contents are not validated.");
    app->add_option(
        "-o, --out", args.outFile, 
        "The file to which all the failed checks are output. If unspecified, "
        "the failed checks are output to stdout; if \"stderr\", terminal "
        "coloring will be suppressed.")
        ->type_name("FILE");
    app->add_flag(
        "--noAssetChecks", args.noAssetChecks, 
        "If specified, do NOT perform extra checks to help ensure the stage or "
        "package can be easily and safely referenced into aggregate stages.");
    app->add_flag(
        "--arkit", args.arkit, 
        "Check if the given USD stage is compatible with RealityKit's "
        "implementation of USDZ as of 2023. These assets operate under greater "
        "constraints that usdz files for more general in-house uses, and this "
        "option attempts to ensure that these constraints are met.");
    app->add_flag(
        "-d, --dumpRules", args.dumpRules, "Dump the enumerated set of rules "
        "being checked for the given set of options.");
    app->add_flag(
        "-v, --verbose", args.verbose, 
        "Enable verbose output mode.");
    app->add_flag(
        "-t, --strict", args.strict, 
        "Return failure code even if only warnings are issued, for stricter "
        "compliance.");
}

static
bool ValidateArgs(const Args& args) {
    // Check conflicting options
    if (args.rootPackageOnly && args.skipVariants) {
        std::cerr<<"Error: The options --rootPackageOnly and --skipVariants "
            "are mutually exclusive.\n";
        return false;
    }

    if (!args.dumpRules && args.inputFile.empty()) {
        std::cerr<<"Error: Either an inputFile or the --dumpRules option "
            "must be specified.\n";
        return false;
    }

    return true;
}

using StringVector = std::vector<std::string>;

static 
int 
UsdChecker(const Args& args) {
    if (!ValidateArgs(args)) {
        return 1;
    }

    #ifndef PXR_PYTHON_SUPPORT_ENABLED

    std::cerr<<"usdchecker using UsdUtilsComplianceChecker requires Python "
        "support to be enabled in the OpenUSD build. Please rebuild USD with "
        "Python support enabled.\n";
    return 1;

    #else

    try {
        TfPyInitialize();
        TfPyLock gil;

        TfPyKwArg arkitArg("arkit", args.arkit);
        TfPyKwArg skipARKitRootLayerCheck("skipARKitRootLayerCheck", false);
        TfPyKwArg rootPackageOnlyArg("rootPackageOnly", args.rootPackageOnly);
        TfPyKwArg skipVariantsArg("skipVariants", args.skipVariants);
        TfPyKwArg verboseArg("verbose", args.verbose);
        TfPyKwArg assetLevelChecks("assetLevelChecks", !args.noAssetChecks);

        object checker;
        if (!TfPyInvokeAndReturn(
                "pxr.UsdUtils", "ComplianceChecker", &checker, arkitArg, 
                skipARKitRootLayerCheck, rootPackageOnlyArg, 
                skipVariantsArg, verboseArg, assetLevelChecks)) {
            std::cerr<<"Error: Failed to initialize ComplianceChecker.\n"; 
            return 1;
        }

        if (!checker) {
            std::cerr<<"Error: Failed to initialize ComplianceChecker.\n"; 
            return 1;
        }

        if (args.dumpRules) {
            checker.attr("DumpRules")();
            // If there's no input file to check, exit after dumping the rules.
            if (args.inputFile.empty()) {
                return 0;
            }
        }

        // We must have an input file to check, based on the validation above
        checker.attr("CheckCompliance")(args.inputFile);

        // Extract warnings, errors, and failed checks from ComplianceChecker.
        StringVector warnings = extract<StringVector>(
            checker.attr("GetWarnings")());
        StringVector errors = extract<StringVector>(
            checker.attr("GetErrors")());
        StringVector failedChecks = extract<StringVector>(
            checker.attr("GetFailedChecks")());

        std::ofstream outFileStream;
        std::ostream &output = [&]() -> std::ostream & {
            if (args.outFile == "stderr") {
                return std::cerr;
            }
            if (args.outFile == "stdout") {
                return std::cout;
            }
            outFileStream.open(args.outFile);
            if (!outFileStream) {
                std::cerr<<"Error: Failed to open output file "<<args.outFile
                    <<" for writing.\n";
            }
            return outFileStream;
        }();

        if (!output) {
            return 1;
        }

        // lambda to print message taking output stream into account and color
        // as an argument.
        auto _PrintMessage = [&output](const std::string& msg, 
                                       const std::string& color) {
            // use ArchFileIsaTTY to check if the output stream is a terminal
            // and if so, color the message. color for cout and cerr.
            if ( (&output == &std::cout && ArchFileIsaTTY(fileno(stdout))) ||
                 (&output == &std::cerr && ArchFileIsaTTY(fileno(stderr))) ){
                output<<color<<msg<<"\033[0m\n";
            } else {
                output<<msg<<'\n';
            }
        };

        // print warnings, errors and failed checks using the above lambda.
        for (const auto& warning : warnings) {
            _PrintMessage(warning, "\033[93m");
        }
        for (const auto& error : errors) {
            _PrintMessage(error, "\033[91m");
        }
        for (const auto& failedCheck : failedChecks) {
            _PrintMessage(failedCheck, "\033[91m");
        }

        if (args.strict && (!warnings.empty() || !errors.empty() || 
                !failedChecks.empty())) {
            _PrintMessage("Failed!\n", "\033[91m");
            return 1;
        }

        if (!errors.empty() || !failedChecks.empty()) {
            _PrintMessage("Failed!\n", "\033[91m");
            return 1;
        }

        if (!warnings.empty()) {
            _PrintMessage("Success with warnings...", "\033[93m");
        } else {
            _PrintMessage("Success!", "\033[92m");
        }
    } catch (error_already_set const&) {
        PyErr_Print();
        std::cerr<<"Error: An exception occurred while running the compliance "
            "checker.\n";
        return 1;
    }
    return 0;

    #endif
}

int main(int argc, char const *argv[]) {
    CLI::App app(
        "Utility for checking the compliance of a given USD stage or a USDZ "
        "package.  Only the first sample of any relevant time-sampled "
        "attribute is checked, currently.  General USD checks are always "
        "performed, and more restrictive checks targeted at distributable "
        "consumer content are also applied when the \"--arkit\" option is "
        "specified.");

    Args args;
    Configure(&app, args);
    CLI11_PARSE(app, argc, argv);
    return UsdChecker(args);
}

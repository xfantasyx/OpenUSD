# Disable usdchecker on non python builds, until new validation framework is in 
# place.
if (NOT PXR_ENABLE_PYTHON_SUPPORT)
    message(STATUS "Skipping usdchecker because python is disabled")
    return()
endif()

# usdchecker currently depends on UsdUtilsComplianceChecker python module, which
# are not build and installed on static builds. We disable usdchecker on static
# builds until we introduce the option to use the new validation framework in a
# subsequent change.
if (NOT BUILD_SHARED_LIBS)
    message(STATUS "Skipping usdchecker because static build is enabled")
    return()
endif()

set(PXR_PREFIX pxr/usd)
set(PXR_PACKAGE usd)

pxr_cpp_bin(usdchecker
    LIBRARIES
        arch
        tf
        sdf
        usd
        usdUtils
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker1
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker2
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker3
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker4
)

pxr_register_test(testUsdChecker1
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker clean/clean.usd"
    EXPECTED_RETURN_CODE 0
)


pxr_register_test(testUsdChecker2
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker clean/clean_flat.usdc"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdChecker3
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker clean/clean.usdz"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdChecker4
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker clean/clean_flat.usdz"
    EXPECTED_RETURN_CODE 0
)


pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker5
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker6
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker7
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker8
)

pxr_register_test(testUsdChecker5
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker --arkit clean/clean_arkit.usdz"
    EXPECTED_RETURN_CODE 0
)


pxr_register_test(testUsdChecker6
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker --arkit clean/clean_flat.usdc"
    EXPECTED_RETURN_CODE 1
)

# Adding a "rootPackageOnly" option to the command line will make the clean files
pxr_register_test(testUsdChecker7
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker --arkit --rootPackageOnly clean/clean_flat.usdz"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdChecker8
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker --arkit clean/clean_flat.usdz"
    EXPECTED_RETURN_CODE 1
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker9
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker10
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker11
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker12
)

pxr_register_test(testUsdChecker9
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker bad/badUsdz.usdz"
    EXPECTED_RETURN_CODE 1
)


pxr_register_test(testUsdChecker10
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker --arkit --dumpRules bad/variants.usdc -o variants_failedChecks.txt"
    DIFF_COMPARE variants_failedChecks.txt
    EXPECTED_RETURN_CODE 1
)

pxr_register_test(testUsdChecker11
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker bad/brokenRef.usd"
    EXPECTED_RETURN_CODE 1
)

pxr_register_test(testUsdChecker12
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker --arkit bad/badShaderUnsupportedTexture.usdc"
    EXPECTED_RETURN_CODE 1
)

pxr_install_test_dir(
    SRC testenv/testUsdChecker
    DEST testUsdChecker13
)

pxr_register_test(testUsdChecker13
    COMMAND "${CMAKE_INSTALL_PREFIX}/bin/usdchecker clean/cleanNormalMapReader.usda"
    EXPECTED_RETURN_CODE 0
)

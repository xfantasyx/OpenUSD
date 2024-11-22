set(DIRS
    usdValidation
    usdGeomValidators
    usdShadeValidators
    usdSkelValidators
    usdUtilsValidators
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()

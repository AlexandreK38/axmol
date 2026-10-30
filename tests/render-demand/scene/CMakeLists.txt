set(APP_NAME render-demand-smoke)
if(ANDROID)
    add_library(${APP_NAME} SHARED Main.cpp)
    target_link_libraries(${APP_NAME} -Wl,--whole-archive cpp_android_spec -Wl,--no-whole-archive)
else()
    if(IOS)
        set_source_files_properties(Main.cpp PROPERTIES LANGUAGE OBJCXX)
    endif()
    add_executable(${APP_NAME} Main.cpp)
endif()
target_link_libraries(${APP_NAME} ${_AX_CORE_LIB})
ax_setup_app_config(${APP_NAME} CONSOLE)
if(WASM)
    # This test creates all content in memory; only the built-in shaders need embedding.
    set(_APP_RES_FOLDER "")
endif()
ax_setup_app_props(${APP_NAME})
if(IOS)
    set_target_properties(${APP_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "dev.axmol.render-demand-smoke"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
endif()

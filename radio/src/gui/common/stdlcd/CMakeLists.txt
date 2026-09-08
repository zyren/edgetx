set(GUI_SRC
  ${GUI_SRC}
  ../common/stdlcd/menu_model.cpp
  ../common/stdlcd/menu_radio.cpp
  ../common/stdlcd/menus_common.cpp
  ../common/stdlcd/draw_functions.cpp
  ../common/stdlcd/fonts.cpp
  ../common/stdlcd/utf8.cpp
  ../common/stdlcd/popups_common.cpp
  ../common/stdlcd/model_inputs.cpp
  ../common/stdlcd/model_mixes.cpp
  ../common/stdlcd/view_text.cpp
  ../common/stdlcd/model_notes.cpp
  ../common/stdlcd/model_curves.cpp
  ../common/stdlcd/radio_hardware.cpp
  ../common/stdlcd/radio_version.cpp
  ../common/stdlcd/radio_trainer.cpp
  ../common/stdlcd/view_telemetry.cpp
  ../common/stdlcd/calibration.cpp
  ../common/stdlcd/splash.cpp
  ../common/stdlcd/radio_sdmanager.cpp
  ../common/stdlcd/widgets_common.cpp
  ../common/stdlcd/lcd_common.cpp
  model_curve_edit.cpp
  )

# Phase 2 CN assets are firmware/simulator sources only. Bootloader sources are
# assembled separately and never receive these files or this feature macro.
# 1 MB-flash X7 parts (and native/simulator builds) embed the full CJK subset.
# 512K-flash X7 parts build with EDGETX_CN_STDLCD_LITE: only the glyphs that
# the SD-card font cannot cover stay embedded; all CJK text is served by
# /FONTS/CN_BASIC.FNT loaded at runtime.
if(PCB STREQUAL X7 AND TRANSLATIONS STREQUAL CN)
  set(GUI_SRC ${GUI_SRC}
    ../../fonts/cn/generated/cn_10.cpp
  )
  if(EDGETX_CN_USE_LITE_FONTS)
    set(GUI_SRC ${GUI_SRC}
      ../../fonts/cn/generated/cn_default_10_lite.cpp
      ../../fonts/cn/generated/cn_12_lite.cpp
      ../../fonts/cn/generated/cn_16_lite.cpp
    )
  else()
    set(GUI_SRC ${GUI_SRC}
      ../../fonts/cn/generated/cn_default_10.cpp
      ../../fonts/cn/generated/cn_12.cpp
      ../../fonts/cn/generated/cn_16.cpp
    )
  endif()
endif()

if(PXX2 OR LUA OR MULTIMODULE)
  set(GUI_SRC ${GUI_SRC} ../common/stdlcd/radio_tools.cpp)
endif()

if(HELI)
  set(GUI_SRC ${GUI_SRC} ../common/stdlcd/model_heli.cpp)
endif()

if(PXX2)
  set(GUI_SRC
    ${GUI_SRC}
    ../common/stdlcd/radio_power_meter.cpp
    )
endif()

if(GHOST)
  set(GUI_SRC
    ${GUI_SRC}
    ../common/stdlcd/radio_ghost_menu.cpp
    )
endif()

# 512K-flash X7 CN firmware (EDGETX_CN_STDLCD_LITE) omits the spectrum analyser
# tool GUI to fit the flash; 1MB parts and native/simulator builds keep it.
if((PXX2 OR MULTIMODULE) AND NOT EDGETX_CN_USE_LITE_FONTS)
  set(GUI_SRC
    ${GUI_SRC}
    ../common/stdlcd/radio_spectrum_analyser.cpp
    )
endif()

if(PXX2)
  set(GUI_SRC
    ${GUI_SRC}
    ../common/stdlcd/model_module_options.cpp
    ../common/stdlcd/model_receiver_options.cpp
    )
endif()

if(USBJ_EX)
  set(GUI_SRC
    ${GUI_SRC}
    ../common/stdlcd/model_usbjoystick.cpp
    )
endif()


/**
 *
 * Internal Emerald scene viewer (kbi/elude @2014-2015)
 *
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define CAMERA_SETTING_Z_FAR (100.0f)
#define MOVEMENT_DELTA       (0.15f)
#define WINDOW_WIDTH         (1920)
#define WINDOW_HEIGHT        (1080)

#define ENABLE_ANIMATION
//#define ENABLE_BB_VISUALIZATION
//#define ENABLE_SM

#ifdef ENABLE_SM
    #define SHOW_SM_PREVIEW
#endif

#endif /* APP_CONFIG_H */
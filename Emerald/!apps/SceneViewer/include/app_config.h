/**
 *
 * Internal Emerald scene viewer (kbi/elude @2014-2015)
 *
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define CAMERA_SETTING_Z_FAR (20.0f)
#define ENABLE_ANIMATION
#define ENABLE_SM

#ifdef ENABLE_SM
    #define SHOW_SM_PREVIEW
#endif

#define WINDOW_WIDTH  (1280)
#define WINDOW_HEIGHT (720)

#endif /* APP_CONFIG_H */
/*
 * LWSDK Header File
 * Copyright 2003,  NewTek, Inc.
 *
 * LWCOLORSPACE.H -- LightWave Color Space
 *
 * Jamie L. Finch
 * Senile Programmer
 */

#ifndef LWSDK_COLORSPACE_H
#define LWSDK_COLORSPACE_H

#ifdef __cplusplus
extern "C" {
#endif

#define LWCOLORSPACEFUNCS_GLOBAL "LWColorSpace"

/* Color Space Names. */

#define LW_COLORSPACE_VIEWER            "ColorSpaceViewer"
#define LW_COLORSPACE_SURFACE_COLOR     "ColorSpaceSurfaceColor"
#define LW_COLORSPACE_LIGHT_COLOR       "ColorSpaceLightColor"
#define LW_COLORSPACE_PALETTE_FILES     "ColorSpacePaletteFiles"
#define LW_COLORSPACE_8BIT_FILES        "ColorSpace8BitFiles"
#define LW_COLORSPACE_FLOAT_FILES       "ColorSpaceFloatFiles"
#define LW_COLORSPACE_ALPHA             "ColorSpaceAlpha"
#define LW_COLORSPACE_OUTPUT            "ColorSpaceOutput"
#define LW_COLORSPACE_OUTPUT_ALPHA      "ColorSpaceOutputAlpha"
#define LW_COLORSPACE_AUTO_SENSE        "ColorSpaceAutoSense"
#define LW_COLORSPACE_CORRECT_OPENGL    "ColorSpaceCorrectOpenGL"
#define LW_COLORSPACE_AFFECT_PICKER     "ColorSpaceAffectPicker"
#define LW_COLORSPACE_8BIT_TO_FLOAT     "ColorSpace8BitToFloat"

typedef enum en_lwcolorspace {
    lwcs_linear = 0,                /*!< LightWave linear color space.      */
    lwcs_sRGB,                      /*!< Standard RGB color space.          */
    lwcs_rec709,                    /*!< Recommendation BT.709, HDTV.       */
    lwcs_cineon,                    /*!< Eastman Kodak Co.                  */
    lwcs_ciexyz,                    /*!< CIE XYZ.                           */
    lwcs_sizeof
} LWCOLORSPACE;

typedef enum en_lwcolorspacetypes {
    lwcst_viewer = 0,               /*!< Viewer color space.                */
    lwcst_surface_color,            /*!< Surface       color space.         */
    lwcst_palette_files,            /*!< Palette files color space.         */
    lwcst_8bit_files,               /*!< 8 bit   files color space.         */
    lwcst_float_files,              /*!< Float   files color space.         */
    lwcst_alpha_files,              /*!< Alpha   files color space.         */
    lwcst_output,                   /*!< Output  files color space.         */
    lwcst_output_alpha,             /*!< Alpha   files color space.         */
    lwcst_light_color,              /*!< Light         color space.         */
    lwcst_sizeof
} LWCOLORSPACETYPES;

typedef enum en_lwcolorspaceconversion {
    lwcsc_colorspaceToLinear = 0,   /*!< Convert from non-linear to linear. */
    lwcsc_linearToColorspace,       /*!< Convert from linear to non-linear. */
    lwcsc_sizeof
} LWCOLORSPACECONVERSION;

typedef enum en_lwcolorspacelayer {
    lwcsl_RGB = 0,                  /*!< RGB   Channel.                     */
    lwcsl_Alpha,                    /*!< Alpha Channel.                     */
    lwcsl_sizeof
} LWCOLORSPACELAYER;

/**
 *      struct st_lwimagelookup *
 *
 *      Is a forward reference to the internal lw instance structure ( defined internally ).
 *      This is used instead of a void * pointer.
 */

typedef void LWPIXELCONVERSIONRGB(   struct st_lwimagelookup *, float *, float * ); /*!< Conversion function prototype. */
typedef void LWPIXELCONVERSIONALPHA( struct st_lwimagelookup *, float *, float * ); /*!< Conversion function prototype. */

/**
 *      Color space functions.
 *
 *      Color spaces file references are save in the scene file if they are used,
 *      in the viewer, color space file defaults or in a LWO from the image viewer.
 */

typedef struct st_lwcolorspacefuncs {

    /* These function are used to get the color space of a loaded converter or load one from the disk. */

    LWCOLORSPACE            (*nameToColorSpace       )( char * );               /*!< Color space name. */
    LWCOLORSPACE            (*loadPixelLookupTable   )( char * );               /*!< File name.        */

    /* These functions are used to get the pixel conversion functions. */

    LWPIXELCONVERSIONRGB   *(*getPixelConversionRGB  )( LWCOLORSPACE, LWCOLORSPACECONVERSION, struct st_lwimagelookup ** );
    LWPIXELCONVERSIONALPHA *(*getPixelConversionAlpha)( LWCOLORSPACE, LWCOLORSPACECONVERSION, struct st_lwimagelookup ** );

    /* These functions are used to get color space names and file names, based on type. */

    const char             *(*colorSpaceName         )( LWCOLORSPACETYPES );    /*!< Gets the name of the current color space of the type selected. */
    const char             *(*colorSpaceFile         )( LWCOLORSPACETYPES );    /*!< Gets the file of the current color space of the type selected. */

    /* These functions are used in pop-ups. */

    int                     (*numberOfColorSpaces    )( LWCOLORSPACELAYER );    /*!< Gets the number of loaded color spaces in the layer.           */
    const char             *(*nameOfColorSpaces      )( LWCOLORSPACELAYER, int);/*!< Gets the name of the color space in the layer.                 */
    LWCOLORSPACE            (*indexToColorSpace      )( LWCOLORSPACELAYER, int);/*!< Used to convert from indexes of pop-ups to color spaces.       */
    int                     (*colorSpaceToIndex      )( LWCOLORSPACELAYER, LWCOLORSPACE );

    /* This functions is to change the currently selected color space. */

    int                     (*setColorSpace          )( LWCOLORSPACETYPES, LWCOLORSPACE );

} LWColorSpaceFuncs;

#ifdef __cplusplus
}
#endif

#endif

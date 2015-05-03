/*
 * LWSDK Header File
 * Copyright 2010,  NewTek, Inc.
 *
 * LWVIEWPORTOBJECT.H -- LightWave Viewport Object
 *
 * Jamie L. Finch
 * Senile Programmer
 */
#ifndef LWSDK_VIEWPORTOBJECT_H
#define LWSDK_VIEWPORTOBJECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lwserver.h>
#include <lwgeneric.h>
#include <lwhandler.h>
#include <lwrender.h>

/* Viewport object window handler. */

#define LWVIEWPORTOBJECT_HCLASS  "ViewportObjectHandler"
#define LWVIEWPORTOBJECT_ICLASS  "ViewportObjectInterface"
#define LWVIEWPORTOBJECT_VERSION 3

/* Rendering Flags. */

typedef enum en_lwviewflags {
    LWVF_a = 0,                 /*!< Buffer has Alpha in it.*/
    LWVF_z,                     /*!< Z Buffer is valid.     */
    LWVF_sizeof
} LWViewFlags;


typedef struct st_LWViewportObjectAccess {
    int          (*showFrame   )( struct st_LWVPAHostAccess *, unsigned int, unsigned int, float *, unsigned int, float *, unsigned int, unsigned int, int, int, int );
} LWViewportObjectAccess;

typedef struct st_LWViewportHandler {
    LWInstanceFuncs *inst;
    LWItemFuncs     *item;
    int          (*newTime     )( LWInstance, double );                             /*!< Time.                                     */
    int          (*newSize     )( LWInstance, unsigned int, unsigned int );         /*!< Width and Height.                         */
    int          (*redraw      )( LWInstance );                                     /*!< Redraws the window.                       */
    int          (*setViewIndex)( LWInstance, unsigned int );                       /*!< View 0 to 3.                              */
    int          (*setViewMode )( LWInstance, unsigned int );                       /*!< LVVIEWT.                                  */
    int          (*setAccess   )( LWInstance, LWViewportObjectAccess *, struct st_LWVPAHostAccess * );
    unsigned int (*flags       )( LWInstance );                                     /*!< LWViewFlags.                              */
    int          (*start       )( LWInstance, unsigned int );                       /*!< Start rendering.                          */
    int          (*stop        )( LWInstance );                                     /*!< Done, free memory.                        */
    LWError      (*selected    )( LWInstance, int );                                /*!< Currently selected or unselected.         */
    int          (*cleanup     )( LWInstance );                                     /*!< Cleanup after redraw.                     */

    /* Layout pull down menu. */

    int          (*menuCount   )( LWInstance );                                     /*!< Count of entries in the menu.             */
    const char * (*menuName    )( LWInstance, unsigned int );                       /*!< Name  of menu entry.                      */
    int          (*menuGetPick )( LWInstance, unsigned int );                       /*!< Menu set entry selected.                  */
    int          (*menuSetPick )( LWInstance, unsigned int, int );                  /*!< Menu get entry selected.                  */

    void         (*inputHandling)( LWInstance, struct LWInputHandling* handler );   /*!< Returns the input handler                 */
} LWViewportHandler;

/* Viewport object camera global. */

#define LWVIEWPORTCAMERA "LW Viewport Camera"

typedef enum {
    LWVPOC_Disabled = 0,
    LWVPOC_Enabled_Orthographic = 1,
    LWVPOC_Enabled_Perspective = 2
} LWViewportObjectCameraEnabled;

typedef struct st_LWViewportObjectCamera {
    double         filmSize;                                                        /*!< Film size for viewport.                   */
    double         focalLength;                                                     /*!< Focal length of viewport.                 */
    double         aspect;                                                          /*!< Pixel aspect ratio of viewport.           */
    double         zNear;                                                           /*!< z Near clipping plane.                    */
    double         zFar;                                                            /*!< z Far  clipping plane.                    */
    LWDMatrix4     modelMatrix;                                                     /*!< OpenGL model view matrix for viewport.    */
    LWDMatrix4     modelMatrixInverted;                                             /*!< Inverse of the model view matrix.         */
    LWDMatrix4     projectionMatrix;                                                /*!< OpenGL projection matrix for viewport.    */
    LWDMatrix4     projectionMatrixInverted;                                        /*!< Inverse of the projection matrix.         */
    LWDMatrix4     viewMatrix;                                                      /*!< Combined model and projection view matrix.*/
    LWDMatrix4     viewMatrixInverted;                                              /*!< Inverse of the view matrix.               */
    float          colorBackground[ 4 ];                                            /*!< Back ground clear color.                  */
    int          (*getRay      )( struct st_LWViewportObjectCamera *, double, double, double *, double * ); /*!< Ray tracing position and direction. */
    int            enabled;                                                         /*!< >0 for supported viewport camera modes.   */
    LWDVector      origin;                                                          /*!< Camera origin in world coordinates.       */
    int          (*getOrigin   )( struct st_LWViewportObjectCamera *, double * );   /*!< Camera world space origin. */
} LWViewportObjectCamera;

/* LightWave function global proto type.    */
/* The global returns this function pointer.*/

typedef LWViewportObjectCamera *LWViewportCamera( unsigned int, unsigned int, int );

/* Viewport object menu global. */

/* Menu Flags. */

typedef enum en_lwviewport_flags {
    LWVPF_openglWireframe = 0,  /*!< Displays wireframe over viewport object. */
    LWVPF_openglOverlay,        /*!< Displays OpenGL overlays.                */
    LWVPF_sizeof
} LWViewportFlags;

#define LWVIEWPORTMENU "LW Viewport Menu"

typedef struct st_LWViewportObjectMenu {
    unsigned int (*getFlags  )( unsigned int );                                     /*!< View 0 to 3.                              */
    int          (*setFlags  )( unsigned int, unsigned int );                       /*!< View 0 to 3, LWViewportFlags.             */
} LWViewportObjectMenu;

#ifdef __cplusplus
}
#endif

#endif

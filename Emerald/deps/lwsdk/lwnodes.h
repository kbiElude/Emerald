/*
* LWSDK Header File
* Copyright 2005, NewTek, Inc.
*
* LWNODES.H -- LightWave Shader Nodes.
*/

#ifndef LWSDK_SURFNODES_H
#define LWSDK_SURFNODES_H

#include <lwserver.h>
#include <lwrender.h>
#include <lwtxtr.h>
#include <lwinstancing.h>

#define LWNODE_HCLASS           "NodeHandler"
#define LWNODE_ICLASS           "NodeInterface"
#define LWNODE_GCLASS           "NodeGizmo"
#define LWNODECLASS_VERSION     3

#define LWNODEFUNCS_GLOBAL          "NodeFuncs"
#define LWNODEINPUTFUNCS_GLOBAL     "NodeInputFuncs 3"
#define LWNODEOUTPUTFUNCS_GLOBAL    "NodeOutputFuncs 2"
#define LWNODEUTILITYFUNCS_GLOBAL   "NodeUtilityFuncs"
#define LWNODEDRAWFUNCS_GLOBAL      "NodeDrawFuncs"

// Predefined server strings for SRVTAG_NODECONTEXT server tag.
#define NCON_ALL  "All"      // Node is available in all contexts.
#define NCON_MESH "Mesh"     // Node is available in contexts where a physical mesh is provided.
#define NCON_SPOT "Spot"     // Node is available in contexts where virtual spot geometry is provided.
#define NCON_SURF "Surface"  // Node is available in contexts where virtual or real surface geometry is provided. A surface requires proper normals.
#define NCON_RAYT "Raytrace" // Node is available in contexts where ray-tracing functions are provided.

// The type of the node preview for the nodes editor.
typedef enum { NPT_RENDER = 0, NPT_CUSTOM, NPT_OFF } NodePreviewType;

// Node input/output types.
typedef enum { NOT_COLOR = 1, NOT_SCALAR, NOT_VECTOR, NOT_INTEGER, NOT_FUNCTION, NOT_MATERIAL, NOT_CUSTOM = 1024 } ConnectionType;

typedef void* NodeInputID;
typedef void* NodeOutputID;
typedef void* NodeID;
typedef void* NodeData;
typedef void* NodeValue;

// The LWNodalAccess struct.
typedef struct LWNodalAccess_t {

    int                 sx, sy;

    LWDVector           oPos, wPos;
    LWDVector           gNorm;
    LWDVector           wNorm;
    LWDVector           wNorm0;

    LWDVector           raySource;
    double              rayLength;

    double              spotSize;
    double              cosine;
    double              bumpHeight;

    double              oXfrm[9], wXfrm[9];

    LWIlluminateFunc        *illuminate;
    LWIlluminateSampleFunc  *illuminateSample;
    LWRayTraceFunc          *rayTrace;
    LWRayCastFunc           *rayCast;
    LWRayShadeFunc          *rayShade;
    LWRayTraceModeFunc      *rayTraceMode;

    int                 bounces;

    LWItemID            sourceID;

    LWItemID            objID;
    int                 polNum;

    LWPntID             verts[4];
    float               weights[4];
    float               vertsWPos[4][3];
    LWPolID             polygon;

    double              incomingEta; // Incoming refraction index.
    int                 ResourceContextIndex; // Current CPU thread running.
    int                 flags;

    RandomFloatData     randomData;
    LWRandomFloatFunc   *randomFloat;

    LWIlluminateNormalFunc *illuminateNormal;
    LWIlluminateSampleNormalFunc *illuminateSampleNormal;

    double              subsx, subsy;

    LWDVector           rayDir;

    LWRayTraceDataFunc  *rayTraceData;
    LWRayTraceShadeFunc *rayTraceShade;
    LWRenderData        render;

    LWBufferGetValFunc  *getVal;
    LWBufferSetValFunc  *setVal;

    /* Added Version 3 for AnimUV cycling. */

    int outsiz;
    LWVertexIndexes  *VertexIndexes; // Indexs To Original Polygone Vertex Index and Prespective Weights.

    LWItemInstanceID    instanceID;

    LWSamplerState          samplerState;
    LWGetSamplerRangeFunc   *getSamplerRange;
    LWGetSample2DFunc         *getSample2D;
    LWGetSample1DFunc         *getSample1D;

} LWNodalAccess;

typedef struct LWNodalMaterial_t {
    LWDVector   diffuse;
    LWDVector   specular;
    LWDVector   reflection;
    LWDVector   refraction;
    double      transparency;
} LWNodalMaterial;

// Draw Functions.
// Draws on the preview area of the node.
typedef struct LWNodeDrawFuncs_t {
    void   (*drawPixel)( NodeID node, int c, int x, int y );
    void   (*drawRGBPixel)( NodeID node, int r, int g, int b, int x, int y );
    void   (*drawLine)( NodeID node, int c, int x, int y, int x2, int y2 );
    void   (*drawBox)( NodeID node, int c, int x, int y, int w, int h );
    void   (*drawRGBBox)( NodeID node, int r, int g, int b, int x, int y, int w, int h );
    int    (*textWidth)( NodeID node, char *s );
    int    (*textHeight)( NodeID node, char *s );
    void   (*drawText)( NodeID node, char *s, int c, int x, int y );
    void   (*blitNode)( NodeID node );
} LWNodeDrawFuncs;

// Input event types.
typedef enum LWNodalEvent_t {
    NIE_CONNECT = 0,        // When an output is connected to this input.
    NIE_DISCONNECT,         // When the output was disconnected from this input.
    NIE_INPUTNODEDESTROY    // When the node connected to this input was destroyed.
} LWNodalEvent;

typedef int NodeInputEvent( void *userData, NodeInputID, LWNodalEvent, ConnectionType );
// Receives the user data, the input the function belongs to, the LWNodalEvent,
//and the type of the output connected to the input this function was called for, if the event was NIE_CONNECT,
// otherwise ConnectionType is 0.

// Node flags. Returned by LWNodeHandler->flags
#define NF_TRANSP           (1<<0)   // This flag should be set if a material node might have transparency
#define NF_SINGLE_THREADED  (1<<30)  // Force single-threaded evaluation (for nodal displacement only)

typedef struct LWNodeOGLTextureOutput_t {
    LWImageID   imageID;
    float       uv_coords[2];
    LWFVector   val;
} LWNodeOGLTextureOutput;

// Node handler activation.
typedef struct st_LWNodeHandler {
    LWInstanceFuncs *inst;
    LWItemFuncs     *item;
    LWRenderFuncs   *rend;

    // Evaluation function receives the LWNodalAccess structure.
    // NodeOutputID is the output belonging to this node, and which is being currently asked the value from.
    // NodeValue is the value you need to set with the output functions when setting a value for this evaluation.
    void            (*evaluate)( LWInstance, LWNodalAccess*, NodeOutputID, NodeValue );

    // customPreview is called when the node has NPT_CUSTOM preview type set.
    void            (*customPreview)( LWInstance, int width, int height );

    // GLTextureEvaluate is called when OpenGL needs image map information from a node.
    void            (*GLTextureEvaluate)( LWInstance, LWNodalAccess*, LWNodeOGLTextureOutput* );

    unsigned int    (*flags)( LWInstance );
} LWNodeHandler;

// Functions for node inputs.
typedef struct	LWNodeInputFuncs_t {
    NodeInputID     (*create)( NodeID, ConnectionType, const char* name, NodeInputEvent* ); // Create a new input for the node.
    void            (*destroy)( NodeID, NodeInputID ); // Destroy an input from the node.

    int             (*evaluate)( NodeInputID, LWNodalAccess*, void* value );
    // Evaluate an input.
    // The value filled with a value received from the node evaluated,
    // according to the type of the input evaluated.

    int             (*check)( NodeInputID ); // Check if this input is connected to.
    NodeInputID     (*first)( NodeID ); // Get the first input from a node.
    NodeInputID     (*next)( NodeInputID ); // Get the next input.
    NodeInputID     (*previous)( NodeInputID ); // Get the previous input.
    int             (*numInputs)( NodeID ); // Get the number of inputs for this node.
    NodeInputID     (*byIndex)( NodeID, int );
    // Get an input by it's index number.
    // The index number of the first input is 1.

    int             (*getIndex)( NodeID, NodeInputID );
    // Get the index number of the input.
    // If the input doesn't belong to this node, returns 0.
    // The index number of the first input is 1.

    void            (*disconnect)( NodeID, NodeInputID ); // Disconnect any output from this input.
    NodeID          (*node)( NodeInputID ); // Returns the node this input belongs to.
    NodeOutputID    (*connectedOutput)( NodeInputID ); // Returns the NodeOutputID this input is connected to.
    const char*     (*name)( NodeInputID ); // Returns the name of the NodeInputID.

    NodeInputID     (*createCustom)(NodeID, ConnectionType, const char* name, NodeInputEvent*, unsigned int, unsigned int); // Create custom connection type.
} LWNodeInputFuncs;

typedef struct	LWNodeOutputFuncs_t {
    NodeOutputID    (*create)( NodeID, ConnectionType, const char* name ); // Create a new output for the node.
    void            (*destroy)( NodeID, NodeOutputID ); // Destroy an output from the node.

    void            (*setValue)( NodeValue, void* );
    // Set the value for the output being evaluated.
    // Call from the node evaluation function when rendering.

    void            *(*getValue)( NodeValue ); // Get the pointer to the value cast to the input functions evaluate call.
    ConnectionType  (*getType)( NodeValue ); // Get the type of the connection the value is coming from.

    NodeOutputID    (*first)( NodeID ); // Get the first output from a node.
    NodeOutputID    (*next)( NodeOutputID ); // Get the next output.
    NodeOutputID    (*previous)( NodeOutputID ); // Get the previous output.
    int             (*numInputs)( NodeID ); // Get the number of outputs for this node.

    NodeOutputID    (*byIndex)( NodeID, int );
    // Get an output by it's index number.
    // The index number of the first output is 1.

    int             (*getIndex)( NodeID, NodeOutputID );
    // Get the index number of the output.
    // If the output doesn't belong to this node, returns 0.
    // The index number of the first output is 1.

    NodeID          (*node)( NodeOutputID );
    // Returns the node this output belongs to.

    const char*     (*name)( NodeInputID ); // Returns the name of the NodeOutputID.

    NodeOutputID    (*createCustom)(NodeID, ConnectionType, const char* name, unsigned int, unsigned int); // Create a custom connection.
} LWNodeOutputFuncs;

typedef struct LWNodeFuncs_t {
    const char      *(*nodeName)( NodeID );
    // Get the name for the node in the editor.
    // Will be the name of the node, with it's index number added to it. Ie. "Texture (1)", etc.

    const char      *(*serverUserName)( NodeID ); // Get the server name for this node.
    LWChanGroupID   (*chanGrp)( NodeID ); // Get the channel group for this node.
    void            (*setNodeColor)( NodeID, int[3] ); // Set the color for this node.
    void            (*setNodeColor3)( NodeID, int r, int g, int b ); // Set the color for this node. using separated R, G and B values.
    void            (*setNodePreviewType)( NodeID, NodePreviewType ); // Set the preview type for this node.

    void            (*UpdateNodePreview)( NodeID );
    // Do an immediate interface update for this node.
    // Draws the preview only for this node.
} LWNodeFuncs;

// Blending modes.
typedef enum {  Blend_Normal, Blend_Additive, Blend_Subtractive, Blend_Multiply,
                Blend_Screen, Blend_Darken, Blend_Lighten, Blend_Difference,
                Blend_Negative, Blend_ColorDodge, Blend_ColorBurn, Blend_Red,
                Blend_Green, Blend_Blue
} BlendingMode;

// Node utility functions.
// To help creating new nodes easier, with consistent color blending modes.
typedef struct LWNodeUtilityFuncs_t {
    void    (*Blend)( LWDVector result, LWDVector bg, LWDVector fg, double alpha, BlendingMode ); // Function to use the built-in blending modes.

    void    (*NodeAutosize)( NodeID, LWDVector scale, LWDVector position ); // Scale and position vectors will be filled with the automatic values.

    void    (*PlanarMapping)( LWDVector tPos, int axis, double *u, double *v );
    void    (*SphericalMapping)( LWDVector tPos, int axis, double *u, double *v );
    void    (*CylindricalMapping)( LWDVector tPos, int axis, double *u, double *v );
    void    (*CubicMapping)( LWDVector tPos, int axis, double *u, double *v ); // The axis for cubic mapping is the major axis the surface normal points to.
    void    (*FrontMapping)( LWDVector tPos, double pixelAspect, double *u, double *v, double cameraMatrix[4][4] );
    void    (*ReflectionMapping)( double seamAngle, LWDVector direction, double *u, double *v );
    int     (*UVMapping)( const char* UVMapName, LWNodalAccess*, double *u, double *v ); // Returns true if successful.

    void    (*BuildCameraMatrix)( LWTime in_time, LWItemID cameraID, double cameraMatrix[4][4] ); // Build the transformation matrix for front projection mapping to use.

} LWNodeUtilityFuncs;

#endif

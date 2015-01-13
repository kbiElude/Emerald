/*
* LWSDK Header File
* Copyright 2005, NewTek, Inc.
*
* LWNODEEDITOR.H -- LightWave Node Editor Functions.
*/

#ifndef LWSDK_NODEEDITOR_H
#define LWSDK_NODEEDITOR_H

#include <lwserver.h>
#include <lwrender.h>
#include <lwnodes.h>

#define LWNODEEDITORFUNCS_GLOBAL "NodeEditorFuncs 3"

typedef void nodeAutoSizeFunc( NodeEditorID, void*, double[3][2] );
// Autosize function, which is used when calling NodeAutosize, from LWNodeUtilityFuncs ( lwnodes.h )

typedef void nodeEditorUpdateFunc( NodeEditorID );
// Update function, which is called when "Instance Update" global function is called with LWNODE_HCLASS.


// This is the root ( Destination ) node for the editor.
typedef struct LWRootNode_t {
    NodePreviewType previewType;
    LWError         (*init)( NodeEditorID, int ); // Init for the root node.
    LWError         (*newTime)( NodeEditorID, LWFrame, LWTime ); // Newtime for the root node.
    void            (*cleanup)( NodeEditorID ); // Cleanup for the root node.
    LWXPanelID      (*rootPanel)( NodeEditorID ); // Interface panel function, for the embedded panels.
    LWError         (*rootOptions)( NodeEditorID ); // Options is called when no panel is defined for the root node.
    void            (*rootPreview)( NodeEditorID vinst, LWNodalAccess* na, LWDVector pcolor ); // Preview function to draw the preview sphere for the root node.
    void            (*rootCustomPreview)( NodeEditorID vinst, int w, int h ); // Custom preview function, for doing custom previews for the root node.
} LWRootNode;

typedef struct st_LWNodeEditorFuncs {

    NodeEditorID        (*create)( const char*, const char*, LWRootNode*, void* ); // Create a node editor ID.
    void                (*destroy)( NodeEditorID ); // Destroy a node editor ID.

    void                (*rename)( const char*, NodeEditorID ); // Rename the editor. It is essentially the title for the editor window.

    void                (*setUpdateFunc)( NodeEditorID, nodeEditorUpdateFunc* ); // Set the update function for the editor.

    LWError             (*copy)( NodeEditorID, NodeEditorID ); // Copy the node editor data.
    LWError             (*save)( NodeEditorID, const LWSaveState* ); // Save the node editor data.
    LWError             (*load)( NodeEditorID, const LWLoadState* ); // Load the node editor data.

    // init, newTime, and cleanup are the same as in LWRenderFuncs.
    LWError             (*init)( NodeEditorID, int );
    LWError             (*newTime)( NodeEditorID, LWFrame, LWTime );
    void                (*cleanup)( NodeEditorID );
    // Init needs to be called before any attempt to evaluate the inputs from the destination node.
    // Cleanup needs to be called after rendering is finished.

    NodeInputID         (*addInput)( NodeEditorID, ConnectionType, const char*, NodeInputEvent* ); // Add an input to the root node.
    void                (*remInput)( NodeEditorID, NodeInputID ); // Remove an input from the root node.

    void                (*setEnvGroup)( NodeEditorID, LWChanGroupID ); // Set the envelope group for the editor. All node envelope groups are parented into this group.
    void                (*setAutosize)( NodeEditorID, nodeAutoSizeFunc* ); // Set the autosize function for the editor.

    void                (*setUserData)( NodeEditorID, void* ); // Set the userdata for this editor.
    void                *(*getUserData)( NodeEditorID ); // Get the userdata from this editor.

    int                 (*numberOfNodes)( NodeEditorID ); // Returns the number of nodes in this editor.

    NodeInputID         (*getInputByName)( NodeEditorID, const char* );
    NodeInputID         (*getInputByIndex)( NodeEditorID, int );
    // These return an NodeInputID from the destination node,
    // which can be used to evaluate an input from the destination,
    // using the NodeInputFuncs global evaluate.

    unsigned short int  (*getState)( NodeEditorID ); // Get the state. Enabled/Disabled.
    void                (*setState)( NodeEditorID, unsigned short int ); // Set the state.

    int                 (*OpenNodeEditor)( NodeEditorID ); // Open the editor window.
    int                 (*isOpenNodeEditor)( NodeEditorID ); // Is the node editor open, and set to this NodeEditorID?

    NodeID              (*getRootNodeID)( NodeEditorID ); // Get an NodeID for the root node;

    LWError             (*connect)( NodeOutputID, NodeInputID ); // Create a connection from the output to the input.
    NodeID              (*addNode)( NodeEditorID, const char* ); // Add a node to the flow using a server name.
    void                (*destroyNode)( NodeID ); // Destroy a node.
    void                (*setXY)( NodeID, int, int ); // Set the X and Y coordinates for the node.

    void                (*setContext)( NodeEditorID, const char* ); // Set the context for the Node Editor

    const LWItemID*     (*usedItems)( NodeEditorID ); // Returns the used items for the whole Node Editor and it's nodes.
} LWNodeEditorFuncs;

#endif

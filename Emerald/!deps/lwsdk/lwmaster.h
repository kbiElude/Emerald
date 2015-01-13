/*
 * LWSDK Header File
 * Copyright 1999, NewTek, Inc.
 *
 * LWMASTER.H -- LightWave Master Handlers
 *
 * This header defines the master handler.  This gets notified of changes
 * in the scene and can respond by issuing commands.
 */
#ifndef LWSDK_MASTER_H
#define LWSDK_MASTER_H

#include <lwrender.h>
#include <lwdyna.h>

#define LWMASTER_HCLASS "MasterHandler"
#define LWMASTER_ICLASS "MasterInterface"
#define LWMASTER_GCLASS "MasterGizmo"
#define LWMASTER_VERSION 4


typedef struct st_LWMasterAccess {
  int eventCode;
  void *eventData;

  void *data;
  LWCommandCode (*lookup)(void *, const char *cmdName);
  int (*execute)( void *, LWCommandCode cmd, int argc, const DynaValue *argv, DynaValue *result);
  int (*evaluate) (void *, const char *command);
} LWMasterAccess;

#define LWEVNT_NOTHING      0
#define LWEVNT_COMMAND      1
#define LWEVNT_TIME         2
#define LWEVNT_SELECT       3
#define LWEVNT_RENDER_DONE  4

/* These are available when LWMASTF_RECEIVE_NOTIFICATIONS is set */

/**
indicates item motion is about to be computed.
eventData is the item id being affected or LWITEM_NULL for all items.
*/
#define LWEVNT_NOTIFY_MOTION_UPDATE_STARTING 0x100

/**
indicates item motion is done being computed.
eventData is the item id being affected or LWITEM_NULL for all items.
*/
#define LWEVNT_NOTIFY_MOTION_UPDATE_COMPLETE 0x101

/**
indicates an object's mesh source has been replaced.
This is a more drastic mesh change than just transforming an existing mesh.
eventData is the item id the object whose mesh has changed.
*/
#define LWEVNT_NOTIFY_MESH_REPLACED 0x102

/**
indicates a single mesh is about to be transformed.
eventData is the object item id being affected.
MeshInfo::pntOtherPos() values are not valid at this point.
*/
#define LWEVNT_NOTIFY_MESH_UPDATE_STARTING 0x103

/**
indicates a mesh vertexes represent their 'after morphing' positions.
'eventData' is an item id.
MeshInfo::pntOtherPos() values may be used.
*/
#define LWEVNT_NOTIFY_MESH_UPDATE_AFTERMORPHING 0x104

/**
indicates a mesh vertexes represent their 'after bones' positions.
'eventData' is an item id.
MeshInfo::pntOtherPos() values may be used.
*/
#define LWEVNT_NOTIFY_MESH_UPDATE_AFTERBONES 0x105

/**
indicates a mesh vertexes represent their 'after local displacements' positions.
'eventData' is an item id.
MeshInfo::pntOtherPos() values may be used.
*/
#define LWEVNT_NOTIFY_MESH_UPDATE_AFTERLOCALDISPLACEMENTS 0x106

/**
indicates a mesh vertexes represent their 'after motion' positions.
'eventData' is an item id.
MeshInfo::pntOtherPos() values may be used
*/
#define LWEVNT_NOTIFY_MESH_UPDATE_AFTERMOTION 0x107

/**
indicates a single mesh is done being transformed.
eventData is the object item id that was affected.
MeshInfo::pntOtherPos() values may be used
*/
#define LWEVNT_NOTIFY_MESH_UPDATE_COMPLETE 0x108

/**
indicates a frame is about to be rendered.
eventData is the time being rendered.
*/
#define LWEVNT_NOTIFY_RENDER_FRAME_STARTING 0x109

/**
indicates a frame has completed being rendered.
eventData is the time being rendered.
*/
#define LWEVNT_NOTIFY_RENDER_FRAME_COMPLETE 0x110

/**
indicates a sequence of frames are about to be rendered
*/
#define LWEVNT_NOTIFY_RENDER_STARTING 0x111

/**
indicates a sequence of frames has completed being rendered.
This is the same as LWEVNT_RENDER_DONE with the addition that it occurs in ScreamerNet.
*/
#define LWEVNT_NOTIFY_RENDER_COMPLETE 0x112

/**
indicates a scene is about to be cleared.
Of course, an existing master handler needs to exist to detect this.
eventData is the name of the scene before it was cleared.
*/
#define LWEVNT_NOTIFY_SCENE_CLEAR_STARTING 0x113

/**
indicates a scene has just been cleared.
Only a master handler that survives a scene clear (LAYOUT type)
will still be around to detect this.
eventData is the name of the scene as it exists as a cleared scene.
*/
#define LWEVNT_NOTIFY_SCENE_CLEAR_COMPLETE 0x114

/**
indicates a scene is about to be loaded.
eventData is the name of the scene file being loaded.
*/
#define LWEVNT_NOTIFY_SCENE_LOAD_STARTING 0x115

/**
indicates a scene has just been loaded.
eventData is the name of the scene file that was loaded.
*/
#define LWEVNT_NOTIFY_SCENE_LOAD_COMPLETE 0x116

/**
indicates a scene is about to be saved.
eventData is the name of the scene being saved.
*/
#define LWEVNT_NOTIFY_SCENE_SAVE_STARTING 0x117

/**
indicates a scene has just been saved.
eventData is the name of the scene file that was saved.
*/
#define LWEVNT_NOTIFY_SCENE_SAVE_COMPLETE 0x118

/**
indicates an item has been added.
eventData is the item id that was added.
*/
#define LWEVNT_NOTIFY_ITEM_ADDED 0x119

/**
indicates an item is about to be removed.
eventData is the item id that is going to be removed.
*/
#define LWEVNT_NOTIFY_ITEM_REMOVING 0x11a

/**
indicates an item has had its parent changed.
eventData is the item id that had its parent changed.
*/
#define LWEVNT_NOTIFY_ITEM_REPARENTED 0x11b

/**
indicates a surface was altered in some way.
eventData is a SurfID
*/
#define LWEVNT_NOTIFY_SURFACE_ALTERED 0x11c

/* @defgroup LWEVNT_NOTIFY_MESHPOINTS These are available when LWMASTF_RECEIVE_NOTIFICATIONS & LWMASTF_RECEIVE_NOTIFICATIONS_MESHPOINTS are set
 {
*/
/**
This indicates a change to the world position of a vertex in an object was altered.
'eventData' is an item ID.
The world vertex position can be altered by simple movement or rotation as well.
By looking for constant movement and orientation, one can rule out those as being
the cause of the alteration.
*/
#define LWEVNT_NOTIFY_MESHPOINTS_ALTERED 0x11d

/**
indicates a channel has been added.
eventData is the channel id that was added.
*/
#define LWEVNT_NOTIFY_CHANNEL_ADDED 0x11e /// @todo

/**
indicates a channel is about to be removed.
eventData is the channel id that is going to be removed.
*/
#define LWEVNT_NOTIFY_CHANNEL_REMOVING 0x11f /// @todo

/**
indicates a surface has been added.
eventData is the surface id that was added.
*/
#define LWEVNT_NOTIFY_SURFACE_ADDED 0x120

/**
indicates a surface is about to be removed.
eventData is the LWSurfaceID that is going to be removed.
*/
#define LWEVNT_NOTIFY_SURFACE_REMOVING 0x121

/** Indicates a plugin instance has changed.
Dependencies on that plugin may wish to be updated and
can determine if they truely need to be by receiving this event,
comparing the plugin class, and server name.
The specific value that changed that caused this event is not
pass along however.
eventData is a structure that contains information about the
instance of the plugin being updated.
*/
#define LWEVNT_NOTIFY_PLUGIN_CHANGED 0x122

/**
sent to clients when the user has aborted the current render
by pressing the "Abort" button (or pressing Escape) from the
Layout GUI
*/
#define LWEVNT_NOTIFY_RENDER_ABORTED 0x123

/**
indicates a texture is about to be removed.
eventData is the LWTextureID that is going to be removed.
*/
#define LWEVNT_NOTIFY_TEXTURE_REMOVING 0x124

/**
indicates an image clip is about to be removed.
eventData is the LWImageID that is going to be removed.
*/
#define LWEVNT_NOTIFY_IMAGE_REMOVING 0x125

/**
indicates image clip settings are being altered such that the resulting
image will differ. eventData is the LWImageID that is going to be removed.
*/
#define LWEVNT_NOTIFY_IMAGE_ALTERED 0x126

/**
indicates a surface was altered in some way.
eventData is a LWTextureID
*/
#define LWEVNT_NOTIFY_TEXTURE_ALTERED 0x127

/**
indicates that the internal object list has been reordered in some way
*/
#define LWEVNT_NOTIFY_LIST_UPDATE_OBJECTS 0x128

/**
indicates that the internal light list has been reordered in some way
*/
#define LWEVNT_NOTIFY_LIST_UPDATE_LIGHTS 0x129

/**
indicates that the internal camera list has been reordered in some way
*/
#define LWEVNT_NOTIFY_LIST_UPDATE_CAMERAS 0x12a

/**
indicates that the internal bone list for an object has been reordered
in some way.  eventData is the LWItemID that owns the bones.
*/
#define LWEVNT_NOTIFY_LIST_UPDATE_BONES 0x12b

/**
indicates an object is about to be saved to a disk file.
eventData is the LWItemID of the object item being saved.
*/
#define LWEVNT_NOTIFY_OBJECT_SAVE_STARTING 0x12c

/**
indicates an object has just been saved to a disk file.
eventData is the LWItemID of the object item that was saved.
*/
#define LWEVNT_NOTIFY_OBJECT_SAVE_COMPLETE 0x12d
/**
indicates a viewport renderer like VPR has been activated.
eventData is null.
*/
#define LWEVNT_VIEWPORT_OBJECT_ADDED 0x12e
/**
indicates a viewport renderer like VPR has been removed.
eventData is null.
*/
#define LWEVNT_VIEWPORT_OBJECT_REMOVED 0x12f

/** This is the structure passed along as eventData when the
LWEVENT_NOTIFY_PLUGIN_CHANGED event is sent.
*/
typedef struct st_LWEventNotifyPluginChange
{
  const char* classname;
  const char* servername;
  unsigned int pluginevent;
} LWEventNotifyPluginChange;

/// The plugin event is one of the following:
#define LWEVNT_PLUGIN_UPDATED       0
#define LWEVNT_PLUGIN_CREATED       1
#define LWEVNT_PLUGIN_DESTROYED     2
#define LWEVNT_PLUGIN_ENABLED       3
#define LWEVNT_PLUGIN_DISABLED      4
#define LWEVNT_PLUGIN_DESTROYING    5


/** @} */

typedef struct st_LWMasterHandler {
  LWInstanceFuncs  *inst;
  LWItemFuncs      *item;
  int               type;
  double          (*event) (LWInstance, const LWMasterAccess *);
  unsigned int    (*flags) (LWInstance);
} LWMasterHandler;

#define LWMAST_SCENE    0
  /*
  A scene level master handler gets destroyed when the scene is cleared
  */
#define LWMAST_LAYOUT   1
  /*
  A layout level master handler remains until user removes it or layout is closed
  */

/* 'flags' return choices */
#define LWMASTF_LAYOUT (1<<0)
  /*
  bit 0: Allows changing type to LWMAST_LAYOUT.
  This is required when type is LWMAST_LAYOUT and you want it to stay that way
  */
#define LWMASTF_RECEIVE_NOTIFICATIONS (1<<1)
  /*
  bit 1: Allows reception of LWEVNT_NOTIFY_ events. see above
  */
#define LWMASTF_RECEIVE_NOTIFICATIONS_MESHPOINTS (1<<2)
  /*
  bit 2: required to receive mesh point specific notifications such as when points are changing
  */

#endif
